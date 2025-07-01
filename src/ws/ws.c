

#include "ws.h"
#include "generic/send.h"
#include "http/http_protocol.h"
#include "http/send.h"
#include "utils/log.h"
#include "utils/string_builder.h"
#include "utils/string_helper.h"

#include <b64/b64.h>
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

	HttpHeaderField upgrade_field = { .key = upgrade_header_buffer,
		                              .value = upgrade_header_buffer +
		                                       strlen(upgrade_header_buffer) + 1 };

	stbds_arrput(additional_headers, upgrade_field);

	char* connection_header_buffer = NULL;
	FORMAT_STRING(&connection_header_buffer, return false;
	              , "%s%c%s", "Connection", '\0', "Upgrade");

	HttpHeaderField connection_field = { .key = connection_header_buffer,
		                                 .value = connection_header_buffer +
		                                          strlen(connection_header_buffer) + 1 };

	stbds_arrput(additional_headers, connection_field);

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
	size_t size = 0;
	unsigned char* b64_result = b64_decode_ex(key, strlen(key), &size);
	if(!b64_result) {
		free(b64_result);
		return false;
	}

	free(b64_result);
	return size == // NOLINT(readability-implicit-bool-conversion)
	       EXPECTED_WS_HEADER_SEC_KEY_LENGTH;
}

static const char* const key_accept_constant = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static char* generate_key_answer(const char* sec_key) {

	char* key_to_hash_buffer = NULL;
	FORMAT_STRING(&key_to_hash_buffer, return NULL;, "%s%s", sec_key, key_accept_constant);

	uint8_t* sha1_hash = sha1(key_to_hash_buffer);

	char* result = b64_encode(sha1_hash, SHA1_LEN);

	free(sha1_hash);
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

static const bool send_http_upgrade_required_status_code = true;

int handle_ws_handshake(const HttpRequest* const http_request,
                        const ConnectionDescriptor* const descriptor, SendSettings send_settings) {

	// check if it is a valid Websocket request
	// according to rfc https://datatracker.ietf.org/doc/html/rfc6455#section-2 section 4.2.1.
	NeededHeaderForHandshake found_list = HandshakeHeaderNone;

	char* sec_key = NULL;
	bool from_browser = false;

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
		} else if(strcasecmp(header.key, "origin") == 0) {
			from_browser = true;
		} else {
			// do nothing
		}

		// TODO(Totto): support this optional headers:
		/*
		   8.   Optionally, a |Sec-WebSocket-Protocol| header field, with a list
		        of values indicating which protocols the client would like to
		        speak, ordered by preference.

		   9.   Optionally, a |Sec-WebSocket-Extensions| header field, with a
		        list of values indicating which extensions the client would like
		        to speak.  The interpretation of this header field is discussed
		        in Section 9.1. */
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

	HttpHeaderField upgrade_field = { .key = upgrade_header_buffer,
		                              .value = upgrade_header_buffer +
		                                       strlen(upgrade_header_buffer) + 1 };

	stbds_arrput(additional_headers, upgrade_field);

	char* connection_header_buffer = NULL;
	FORMAT_STRING(&connection_header_buffer, return false;
	              , "%s%c%s", "Connection", '\0', "Upgrade");

	HttpHeaderField connection_field = { .key = connection_header_buffer,
		                                 .value = connection_header_buffer +
		                                          strlen(connection_header_buffer) + 1 };

	stbds_arrput(additional_headers, connection_field);

	char* key_answer = generate_key_answer(sec_key);

	char* sec_websocket_accept_header_buffer = NULL;
	FORMAT_STRING(&sec_websocket_accept_header_buffer, return false;
	              , "%s%c%s", "Sec-WebSocket-Accept", '\0', key_answer);

	free(key_answer);

	HttpHeaderField sec_ws_accept_field = {
		.key = sec_websocket_accept_header_buffer,
		.value = sec_websocket_accept_header_buffer + strlen(sec_websocket_accept_header_buffer) + 1
	};

	stbds_arrput(additional_headers, sec_ws_accept_field);

	HTTPResponseToSend to_send = { .status = HttpStatusSwitchingProtocols,
		                           .body = http_response_body_empty(),
		                           .mime_type = NULL,
		                           .additional_headers = additional_headers };

	return send_http_message_to_connection(descriptor, to_send, send_settings);
}
