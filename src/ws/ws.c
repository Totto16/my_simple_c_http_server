

#include "ws.h"
#include "generic/hash.h"
#include "generic/send.h"
#include "http/http_protocol.h"
#include "http/send.h"
#include "utils/log.h"
#include "utils/string_builder.h"
#include "utils/string_helper.h"

#include <ctype.h>
#include <strings.h>

NODISCARD static int
send_failed_handshake_message_upgrade_required(const ConnectionDescriptor* const descriptor,
                                               SendSettings send_settings) {

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Failed WS handshake: Upgrade required\n");

	StringBuilder* message = string_builder_init();

	string_builder_append_single(message, "Error: The client handshake was invalid: This endpoint "
	                                      "requires an upgrade to the WebSocket protocol");

	HttpHeaderFields additional_headers = STBDS_ARRAY_EMPTY;

	char* upgrade_header_buffer = NULL;
	FORMAT_STRING(&upgrade_header_buffer, return false;, "%s%c%s", "Upgrade", '\0', "WebSocket");

	add_http_header_field_by_double_str(&additional_headers, upgrade_header_buffer);

	char* connection_header_buffer = NULL;
	FORMAT_STRING(&connection_header_buffer, return false;
	              , "%s%c%s", "Connection", '\0', "Upgrade");

	add_http_header_field_by_double_str(&additional_headers, connection_header_buffer);

	HTTPResponseToSend to_send = { .status = HttpStatusUpgradeRequired,
		                           .body = http_response_body_from_string_builder(&message),
		                           .mime_type = MIME_TYPE_TEXT,
		                           .additional_headers = additional_headers };

	int result = send_http_message_to_connection(descriptor, to_send, send_settings);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "Error while sending a response (in send_failed_handshake_message)\n");
	}
	return -1;
}

NODISCARD static int send_failed_handshake_message(const ConnectionDescriptor* const descriptor,
                                                   const char* error_reason,
                                                   SendSettings send_settings) {

	LOG_MESSAGE(LogLevelTrace, "Failed WS handshake: %s\n", error_reason);

	StringBuilder* message = string_builder_init();

	STRING_BUILDER_APPENDF(message, return false;
	                       , "Error: The client handshake was invalid: %s", error_reason);

	HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
		                           .body = http_response_body_from_string_builder(&message),
		                           .mime_type = MIME_TYPE_TEXT,
		                           .additional_headers = STBDS_ARRAY_EMPTY };

	free_string_builder(message);

	int result = send_http_message_to_connection(descriptor, to_send, send_settings);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "Error while sending a response (in send_failed_handshake_message)\n");
	}
	return -1;
}

#define EXPECTED_WS_HEADER_SEC_KEY_LENGTH 16

NODISCARD static bool is_valid_sec_key(const char* key) {
	SizedBuffer b64_result =
	    base64_decode_buffer((SizedBuffer){ .data = (void*)key, .size = strlen(key) });
	if(!b64_result.data) {
		return false;
	}

	free_sized_buffer(b64_result);
	return b64_result.size == // NOLINT(readability-implicit-bool-conversion)
	       EXPECTED_WS_HEADER_SEC_KEY_LENGTH;
}

static const char* const key_accept_constant = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static char* generate_key_answer(const char* sec_key) {

	char* key_to_hash_buffer = NULL;
	FORMAT_STRING(&key_to_hash_buffer, return NULL;, "%s%s", sec_key, key_accept_constant);

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

#define DEFAULT_MAX_WINDOW_BITS 15

#define MIN_MAX_WINDOW_BITS 8
#define MAX_MAX_WINDOW_BITS 15

NODISCARD static bool parse_ws_extension_per_message_deflate_params(char* params,
                                                                    WsDeflateOptions* options) {

	char* current_params = params;

	while(true) {

		char* next_params = index(current_params, ';');

		if(next_params != NULL) {
			*next_params = '\0';
		}

		// strip whitespace
		while(isspace(*current_params)) {
			current_params++;
		}

		{

			char* current_param_name = current_params;

			char* current_param_value = NULL;

			char* current_param_value_start = index(current_param_name, '=');

			if(current_param_value_start != NULL) {
				*current_param_value_start = '\0';
				current_param_value = current_param_value_start + 1;
			}

			if(strcmp(current_param_name, "server_no_context_takeover") == 0) {
				if(current_param_value != NULL) {
					return false;
				}

				options->server.no_context_takeover = true;

			} else if(strcmp(current_param_name, "server_max_window_bits") == 0) {

				uint8_t max_window_bits = DEFAULT_MAX_WINDOW_BITS;

				if(current_param_value != NULL) {
					bool success = true;
					long parsed_number = parse_long(current_param_value, &success);

					if(!success) {
						return false;
					}

					if(parsed_number < MIN_MAX_WINDOW_BITS || parsed_number > MAX_MAX_WINDOW_BITS) {
						return false;
					}

					max_window_bits = (uint8_t)parsed_number;
				}

				options->server.max_window_bits = max_window_bits;

			} else if(strcmp(current_param_name, "client_no_context_takeover") == 0) {
				if(current_param_value != NULL) {
					return false;
				}

				options->client.no_context_takeover = true;

			} else if(strcmp(current_param_name, "client_max_window_bits") == 0) {

				uint8_t max_window_bits = DEFAULT_MAX_WINDOW_BITS;

				if(current_param_value != NULL) {
					bool success = true;
					long parsed_number = parse_long(current_param_value, &success);

					if(!success) {
						return false;
					}

					if(parsed_number < 0 || parsed_number > UINT8_MAX) {
						return false;
					}

					max_window_bits = (uint8_t)parsed_number;
				}

				options->client.max_window_bits = max_window_bits;

			} else {
				return false;
			}
		}

		if(next_params == NULL) {
			break;
		}

		current_params = next_params + 1;
	}

	return true;
}

#define DEFAULT_CONTEXT_TAKEOVER_VALUE false

NODISCARD static WSExtension parse_ws_extension_value(char* value, bool* success) {

	char* name = value;

	char* params_start = index(value, ';');

	char* params = NULL;
	if(params_start != NULL) {
		*params_start = '\0';
		params = params_start + 1;

		// strip whitespace
		while(isspace(*params)) {
			params++;
		}
	}

	if(strcmp(name, "permessage-deflate") == 0) {

		WSExtension extension = {
			.type = WSExtensionTypePerMessageDeflate,
			.data = { .deflate = { .client = { .no_context_takeover =
			                                       DEFAULT_CONTEXT_TAKEOVER_VALUE,
			                                   .max_window_bits = DEFAULT_MAX_WINDOW_BITS },
			                       .server = { .no_context_takeover =
			                                       DEFAULT_CONTEXT_TAKEOVER_VALUE,
			                                   .max_window_bits = DEFAULT_MAX_WINDOW_BITS } } }
		};

		if(params != NULL) {
			bool res =
			    parse_ws_extension_per_message_deflate_params(params, &extension.data.deflate);

			*success = res;
			return extension;
		}

		*success = true;
		return extension;
	}

	*success = false;
	return (WSExtension){};
}

// see https://datatracker.ietf.org/doc/html/rfc6455#section-9.1
static void parse_ws_extensions(WSExtensions* extensions, const char* const value_const) {

	char* value = strdup(value_const);

	char* current_extension = value;

	while(true) {

		char* next_value = index(current_extension, ',');

		if(next_value != NULL) {
			*next_value = '\0';
		}

		bool success = true;

		WSExtension extension = parse_ws_extension_value(current_extension, &success);

		if(success) {
			stbds_arrput(*extensions, extension);
		}

		if(next_value == NULL) {
			break;
		}

		current_extension = next_value + 1;
	}

	free(value);
}

NODISCARD static bool append_ws_extension_as_string(StringBuilder* string_builder,
                                                    WSExtension extension) {

	switch(extension.type) {
		case WSExtensionTypePerMessageDeflate: {

			string_builder_append_single(string_builder, "permessage-deflate");

			WsDeflateOptions options = extension.data.deflate;

			// always return our window sizes
			size_t additional_options_count = 0;

			if(options.client.no_context_takeover) {
				additional_options_count++;
			}

			if(options.server.no_context_takeover) {
				additional_options_count++;
			}

			{

				string_builder_append_single(string_builder, "server_max_window_bits=");

				STRING_BUILDER_APPENDF(string_builder, return false;
				                       , "%d", options.server.max_window_bits);

				string_builder_append_single(string_builder, ";");
			}

			size_t index = 0;
			{

				string_builder_append_single(string_builder, "client_max_window_bits=");

				STRING_BUILDER_APPENDF(string_builder, return false;
				                       , "%d", options.client.max_window_bits);

				if(additional_options_count != index) {
					string_builder_append_single(string_builder, ";");
				}

				++index;
			}

			if(options.server.no_context_takeover) {

				string_builder_append_single(string_builder, "server_no_context_takeover");

				if(additional_options_count != index) {
					string_builder_append_single(string_builder, ";");
				}

				++index;
			}

			if(options.client.no_context_takeover) {

				string_builder_append_single(string_builder, "client_no_context_takeover");
			}

			break;
		}
		default: {
			return false;
		}
	}

	return true;
}

NODISCARD static char* get_accepted_ws_extensions_as_string(WSExtensions extensions) {

	StringBuilder* string_builder = string_builder_init();

	if(!string_builder) {
		return NULL;
	}

	size_t extensions_length = stbds_arrlenu(extensions);

	for(size_t i = 0; i < extensions_length; ++i) {
		WSExtension extension = extensions[i];

		bool success = append_ws_extension_as_string(string_builder, extension);

		if(!success) {
			free_string_builder(string_builder);
			return NULL;
		}

		if(i != extensions_length - 1) {
			string_builder_append_single(string_builder, ",");
		}
	}

	return string_builder_release_into_string(&string_builder);
}

static const bool send_http_upgrade_required_status_code = true;

int handle_ws_handshake(const HttpRequest* const http_request,
                        const ConnectionDescriptor* const descriptor, SendSettings send_settings) {

	// check if it is a valid Websocket request
	// according to rfc https://datatracker.ietf.org/doc/html/rfc6455#section-2 section 4.2.1.
	NeededHeaderForHandshake found_list = HandshakeHeaderNone;

	char* sec_key = NULL;
	bool from_browser = false;
	WSExtensions extensions = STBDS_ARRAY_EMPTY;

	for(size_t i = 0; i < stbds_arrlenu(http_request->head.header_fields); ++i) {
		HttpHeaderField header = http_request->head.header_fields[i];
		if(strcasecmp(header.key, "host") == 0) {
			found_list |= HandshakeHeaderHeaderHost;
		} else if(strcasecmp(header.key, "upgrade") == 0) {
			found_list |= HandshakeHeaderHeaderUpgrade;
			if(strcasecontains(header.value, "websocket") < 0) {
				return send_failed_handshake_message(
				    descriptor, "upgrade does not contain 'websocket'", send_settings);
			}
		} else if(strcasecmp(header.key, "connection") == 0) {
			found_list |= HandshakeHeaderHeaderConnection;
			if(strcasecontains(header.value, "upgrade") < 0) {
				if(send_http_upgrade_required_status_code) {
					return send_failed_handshake_message_upgrade_required(descriptor,
					                                                      send_settings);
				}

				return send_failed_handshake_message(
				    descriptor, "connection does not contain 'upgrade'", send_settings);
			}
		} else if(strcasecmp(header.key, "sec-websocket-key") == 0) {
			found_list |= HandshakeHeaderHeaderSecWebsocketKey;
			if(is_valid_sec_key(header.value)) {
				sec_key = header.value;
			} else {
				return send_failed_handshake_message(descriptor, "sec-websocket-key is invalid",
				                                     send_settings);
			}
		} else if(strcasecmp(header.key, "sec-websocket-version") == 0) {
			found_list |= HandshakeHeaderHeaderSecWebsocketVersion;
			if(strcmp(header.value, "13") != 0) {
				return send_failed_handshake_message(
				    descriptor, "sec-websocket-version has invalid value", send_settings);
			}
		} else if(strcasecmp(header.key, "sec-websocket-extensions") == 0) {
			// TODO(Totto): this header field may be specified multiple times, but we should
			// combine all and than parse it, but lets see if the autobahn test suite tests for
			// that first
			parse_ws_extensions(&extensions, header.value);

		} else if(strcasecmp(header.key, "origin") == 0) {
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
			return send_failed_handshake_message_upgrade_required(descriptor, send_settings);
		}
		return send_failed_handshake_message(descriptor, "missing required headers", send_settings);
	}

	// send server handshake

	HttpHeaderFields additional_headers = STBDS_ARRAY_EMPTY;

	char* upgrade_header_buffer = NULL;
	FORMAT_STRING(&upgrade_header_buffer, return false;, "%s%c%s", "Upgrade", '\0', "WebSocket");

	add_http_header_field_by_double_str(&additional_headers, upgrade_header_buffer);

	char* connection_header_buffer = NULL;
	FORMAT_STRING(&connection_header_buffer, return false;
	              , "%s%c%s", "Connection", '\0', "Upgrade");

	add_http_header_field_by_double_str(&additional_headers, connection_header_buffer);

	char* key_answer = generate_key_answer(sec_key);

	if(key_answer != NULL) {

		char* sec_websocket_accept_header_buffer = NULL;
		FORMAT_STRING(&sec_websocket_accept_header_buffer, return false;
		              , "%s%c%s", "Sec-WebSocket-Accept", '\0', key_answer);

		free(key_answer);

		add_http_header_field_by_double_str(&additional_headers,
		                                    sec_websocket_accept_header_buffer);
	}

	if(stbds_arrlenu(extensions) > 0) {
		char* accepted_extensions = get_accepted_ws_extensions_as_string(extensions);

		if(accepted_extensions != NULL) {

			char* sec_websocket_extensions_header_buffer = NULL;
			FORMAT_STRING(&sec_websocket_extensions_header_buffer, return false;
			              , "%s%c%s", "Sec-WebSocket-Extensions", '\0', accepted_extensions);

			free(accepted_extensions);

			add_http_header_field_by_double_str(&additional_headers,
			                                    sec_websocket_extensions_header_buffer);
		}
	}

	HTTPResponseToSend to_send = { .status = HttpStatusSwitchingProtocols,
		                           .body = http_response_body_empty(),
		                           .mime_type = NULL,
		                           .additional_headers = additional_headers };

	return send_http_message_to_connection(descriptor, to_send, send_settings);
}

NODISCARD static WsFragmentOption get_ws_fragment_args_from_http_request(bool fragmented,
                                                                         ParsedURLPath path) {
	if(!fragmented) {
		return (WsFragmentOption){ .type = WsFragmentOptionTypeOff };
	}

	WsFragmentOption result = { .type = WsFragmentOptionTypeAuto };

	ParsedSearchPathEntry* fragment_size = find_search_key(path.search_path, "fragment_size");

	if(fragment_size != NULL) {

		bool success = true;

		long parsed_long = parse_long(fragment_size->value, &success);

		if(success) {

			if(parsed_long >= 0 && (size_t)parsed_long < SIZE_MAX) {
				result.type = WsFragmentOptionTypeSet;
				result.data.set.fragment_size = (size_t)parsed_long;
			}
		}
	}

	return result;
}

NODISCARD WsConnectionArgs get_ws_args_from_http_request(bool fragmented, ParsedURLPath path) {

	return (WsConnectionArgs){ .fragment_option =
		                           get_ws_fragment_args_from_http_request(fragmented, path) };
}
