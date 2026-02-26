

#include "ws.h"
#include "generic/hash.h"
#include "generic/send.h"
#include "http/header.h"
#include "http/mime.h"
#include "http/protocol.h"
#include "http/send.h"
#include "utils/log.h"
#include "utils/string_builder.h"

#include <strings.h>

NODISCARD static int
send_failed_handshake_message_upgrade_required(const ConnectionDescriptor* const descriptor,
                                               HTTPGeneralContext* general_context,
                                               SendSettings send_settings) {

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Failed WS handshake: Upgrade required\n");

	StringBuilder* message = string_builder_init();

	string_builder_append_single(message, "Error: The client handshake was invalid: This endpoint "
	                                      "requires an upgrade to the WebSocket protocol");

	HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

	{
		add_http_header_field(&additional_headers, HTTP_HEADER_NAME(upgrade),
		                      TSTR_LIT("websocket"));

		add_http_header_field(&additional_headers, HTTP_HEADER_NAME(connection),
		                      TSTR_LIT("upgrade"));
	}

	HTTPResponseToSend to_send = { .status = HttpStatusUpgradeRequired,
		                           .body = http_response_body_from_string_builder(&message, true),
		                           .mime_type = MIME_TYPE_TEXT,
		                           .additional_headers = additional_headers };

	int result =
	    send_http_message_to_connection(general_context, descriptor, to_send, send_settings);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "Error while sending a response (in send_failed_handshake_message)\n");
	}
	return -1;
}

NODISCARD static int send_failed_handshake_message(const ConnectionDescriptor* const descriptor,
                                                   HTTPGeneralContext* general_context,
                                                   const char* error_reason,
                                                   SendSettings send_settings) {

	LOG_MESSAGE(LogLevelTrace, "Failed WS handshake: %s\n", error_reason);

	StringBuilder* message = string_builder_init();

	STRING_BUILDER_APPENDF(message, return false;
	                       , "Error: The client handshake was invalid: %s", error_reason);

	HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
		                           .body = http_response_body_from_string_builder(&message, true),
		                           .mime_type = MIME_TYPE_TEXT,
		                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

	free_string_builder(message);

	int result =
	    send_http_message_to_connection(general_context, descriptor, to_send, send_settings);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "Error while sending a response (in send_failed_handshake_message)\n");
	}
	return -1;
}

#define EXPECTED_WS_HEADER_SEC_KEY_LENGTH 16

NODISCARD static bool is_valid_sec_key(const tstr* const key) {
	SizedBuffer b64_result = base64_decode_buffer(sized_buffer_from_tstr(key));
	if(!b64_result.data) {
		return false;
	}

	free_sized_buffer(b64_result);
	return b64_result.size == // NOLINT(readability-implicit-bool-conversion)
	       EXPECTED_WS_HEADER_SEC_KEY_LENGTH;
}

static const char* const key_accept_constant = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static char* generate_key_answer(const tstr* const sec_key) {

	char* key_to_hash_buffer = NULL;
	FORMAT_STRING(&key_to_hash_buffer, return NULL;
	              , "%s%s", tstr_cstr(sec_key), key_accept_constant);

	SizedBuffer sha1_hash = get_sha1_from_string(key_to_hash_buffer);

	char* result = base64_encode_buffer(sha1_hash);

	free_sized_buffer(sha1_hash);
	free(key_to_hash_buffer);

	return result;
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HandshakeHeaderNone = 0b0,
	HandshakeHeaderHeaderHost = 0b1,
	HandshakeHeaderHeaderUpgrade = 0b10,
	HandshakeHeaderHeaderConnection = 0b100,
	HandshakeHeaderHeaderSecWebsocketKey = 0b1000,
	HandshakeHeaderHeaderSecWebsocketVersion = 0b10000,
	//
	HandshakeHeaderHeaderAllFound = 0b11111,
} NeededHeaderForHandshake;

NODISCARD static int are_extensions_supported(const ConnectionDescriptor* const descriptor,
                                              HTTPGeneralContext* general_context,
                                              SendSettings send_settings, WSExtensions extensions) {

	size_t extension_length = TVEC_LENGTH(WSExtension, extensions);

	if(extension_length == 0) {
		return 0;
	}

	// TODO(Totto): support more extensions
	if(extension_length != 1) {
		return send_failed_handshake_message(descriptor, general_context,
		                                     "only one extension supported atm", send_settings);
	}

	for(size_t i = 0; i < extension_length; ++i) {
		WSExtension extension = TVEC_AT(WSExtension, extensions, i);

		switch(extension.type) {
			case WSExtensionTypePerMessageDeflate: {

				if(!extension.data.deflate.client.no_context_takeover) {
					return send_failed_handshake_message(
					    descriptor, general_context,
					    "client needs to set the option no_context_takeover, takeover not "
					    "supported",
					    send_settings);
				}

				if(!extension.data.deflate.server.no_context_takeover) {
					return send_failed_handshake_message(
					    descriptor, general_context,
					    "server needs to set the option no_context_takeover, takeover not "
					    "supported",
					    send_settings);
				}

#ifndef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
				return send_failed_handshake_message(
				    descriptor,
				    "server not compiled with deflate compression and decompression support",
				    send_settings);
#endif

				break;
			}
			default: {
				return send_failed_handshake_message(descriptor, general_context,
				                                     "unexpected extension found", send_settings);
			}
		}
	}

	return 0;
}

static const bool send_http_upgrade_required_status_code = true;

typedef struct {
	const tstr field_name;
	bool success;
} WsHeaderProcessArg;

static void process_ws_header(const tstr_view value, void* argument) {

	WsHeaderProcessArg* arg = (WsHeaderProcessArg*)argument;

	if(tstr_view_eq_ignore_case(value, tstr_cstr(&(arg->field_name)))) {
		arg->success = true;
	}
}

int handle_ws_handshake(const HttpRequest http_request,
                        const ConnectionDescriptor* const descriptor,
                        HTTPGeneralContext* general_context, SendSettings send_settings,
                        WSExtensions* extensions) {

	// check if it is a valid Websocket request
	// according to rfc https://datatracker.ietf.org/doc/html/rfc6455#section-4.2.1
	NeededHeaderForHandshake found_list = HandshakeHeaderNone;

	tstr sec_key = tstr_init();
	bool from_browser = false;

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, http_request.head.header_fields); ++i) {
		const HttpHeaderField header = TVEC_AT(HttpHeaderField, http_request.head.header_fields, i);

		if(tstr_eq_ignore_case(&header.key, &HTTP_HEADER_NAME(host))) {
			found_list |= HandshakeHeaderHeaderHost;
		} else if(tstr_eq_ignore_case(&header.key, &HTTP_HEADER_NAME(upgrade))) {
			found_list |= HandshakeHeaderHeaderUpgrade;

			WsHeaderProcessArg process_arg = {
				.field_name = TSTR_LIT("websocket"),
				.success = false,
			};

			process_delimitered_header_value(tstr_as_view(&header.value), ",", process_ws_header,
			                                 &process_arg);

			if(!process_arg.success) {
				return send_failed_handshake_message(descriptor, general_context,
				                                     "upgrade does not contain 'websocket'",
				                                     send_settings);
			}
		} else if(tstr_eq_ignore_case(&header.key, &HTTP_HEADER_NAME(connection))) {
			found_list |= HandshakeHeaderHeaderConnection;

			WsHeaderProcessArg process_arg = {
				.field_name = HTTP_HEADER_NAME(upgrade),
				.success = false,
			};

			process_delimitered_header_value(tstr_as_view(&header.value), ",", process_ws_header,
			                                 &process_arg);

			if(!process_arg.success) {
				if(send_http_upgrade_required_status_code) {
					return send_failed_handshake_message_upgrade_required(
					    descriptor, general_context, send_settings);
				}

				return send_failed_handshake_message(descriptor, general_context,
				                                     "connection does not contain 'upgrade'",
				                                     send_settings);
			}
		} else if(tstr_eq_ignore_case(&header.key, &HTTP_HEADER_NAME(ws_sec_websocket_key))) {
			found_list |= HandshakeHeaderHeaderSecWebsocketKey;
			if(is_valid_sec_key(&header.value)) {
				sec_key = header.value;
			} else {
				return send_failed_handshake_message(descriptor, general_context,
				                                     "sec-websocket-key is invalid", send_settings);
			}
		} else if(tstr_eq_ignore_case(&header.key, &HTTP_HEADER_NAME(ws_sec_websocket_version))) {
			found_list |= HandshakeHeaderHeaderSecWebsocketVersion;
			if(!tstr_eq_cstr(&header.value, "13")) {
				return send_failed_handshake_message(descriptor, general_context,
				                                     "sec-websocket-version has invalid value",
				                                     send_settings);
			}
		} else if(tstr_eq_ignore_case(&header.key,
		                              &HTTP_HEADER_NAME(ws_sec_websocket_extensions))) {
			// TODO(Totto): this header field may be specified multiple times, but we should
			// combine all and than parse it, but lets see if the autobahn test suite tests for
			// that first
			// TODO: normalize headers in some place!
			parse_ws_extensions(extensions, tstr_as_view(&header.value));

		} else if(tstr_eq_ignore_case(&header.key, &HTTP_HEADER_NAME(origin))) {
			from_browser = true;
		} else {
			// do nothing
			continue;
		}

		// TODO(Totto): support this optional headers:
		/*
		   8.   Optionally, a |Sec-WebSocket-Protocol| header field, with a list
		        of values indicating which protocols the client would like to
		        speak, ordered by preference.
		*/
	}

	UNUSED(from_browser);

	if((HandshakeHeaderHeaderAllFound & found_list) != HandshakeHeaderHeaderAllFound) {
		if(send_http_upgrade_required_status_code && /*NOLINT(readability-implicit-bool-conversion)*/
		   ((found_list & HandshakeHeaderHeaderUpgrade) == 0)) {
			return send_failed_handshake_message_upgrade_required(descriptor, general_context,
			                                                      send_settings);
		}
		return send_failed_handshake_message(descriptor, general_context,
		                                     "missing required headers", send_settings);
	}

	if(are_extensions_supported(descriptor, general_context, send_settings, *extensions) < 0) {
		return -1;
	}

	// send server handshake

	HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

	{
		add_http_header_field(&additional_headers, HTTP_HEADER_NAME(upgrade),
		                      TSTR_LIT("websocket"));

		add_http_header_field(&additional_headers, HTTP_HEADER_NAME(connection),
		                      TSTR_LIT("upgrade"));
	}

	char* key_answer = generate_key_answer(&sec_key);

	if(key_answer != NULL) {

		add_http_header_field(&additional_headers, HTTP_HEADER_NAME(ws_sec_websocket_accept),
		                      tstr_own_cstr(key_answer));
	}

	if(!TVEC_IS_EMPTY(WSExtension, *extensions)) {
		char* accepted_extensions = get_accepted_ws_extensions_as_string(*extensions);

		if(accepted_extensions != NULL) {

			add_http_header_field(&additional_headers,
			                      HTTP_HEADER_NAME(ws_sec_websocket_extensions),
			                      tstr_own_cstr(accepted_extensions));
		}
	}

	HTTPResponseToSend to_send = { .status = HttpStatusSwitchingProtocols,
		                           .body = http_response_body_empty(),
		                           .mime_type = NULL,
		                           .additional_headers = additional_headers };

	return send_http_message_to_connection(general_context, descriptor, to_send, send_settings);
}

NODISCARD static WsFragmentOption get_ws_fragment_args_from_http_request(ParsedURLPath path) {

	const ParsedSearchPathEntry* fragmented_paramater =
	    find_search_key(path.search_path, TSTR_LIT("fragmented"));

	if(fragmented_paramater == NULL) {
		return (WsFragmentOption){ .type = WsFragmentOptionTypeOff };
	}

	WsFragmentOption result = { .type = WsFragmentOptionTypeAuto };

	const ParsedSearchPathEntry* fragment_size_parameter =
	    find_search_key(path.search_path, TSTR_LIT("fragment_size"));

	if(fragment_size_parameter != NULL) {

		bool success = true;

		long parsed_long =
		    parse_long_tstr(tstr_as_view(&fragment_size_parameter->value.val), &success);

		if(success) {

			if(parsed_long >= 0 && (size_t)parsed_long < SIZE_MAX) {
				result.type = WsFragmentOptionTypeSet;
				result.data.set.fragment_size = (size_t)parsed_long;
			}
		}
	}

	return result;
}

NODISCARD WsConnectionArgs get_ws_args_from_http_request(ParsedURLPath path,
                                                         WSExtensions extensions) {

	const ParsedSearchPathEntry* trace_paramater =
	    find_search_key(path.search_path, TSTR_LIT("trace"));

	return (WsConnectionArgs){ .fragment_option = get_ws_fragment_args_from_http_request(path),
		                       .extensions = extensions,
		                       .trace = trace_paramater != NULL };
}
