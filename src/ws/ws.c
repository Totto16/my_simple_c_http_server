

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
sendFailedHandshakeMessageUpgradeRequired(const ConnectionDescriptor* const descriptor) {

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Failed WS handshake: Upgrade required\n");

	StringBuilder* message = string_builder_init();

	string_builder_append_single(message, "Error: The client handshake was invalid: This endpoint "
	                                      "requires an upgrade to the WebSocket protocol");

	char* malloced_message = string_builder_get_string(message);

	const int headerAmount = 2;

	HttpHeaderField* header =
	    (HttpHeaderField*)mallocWithMemset(sizeof(HttpHeaderField) * headerAmount, true);

	if(!header) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return -1;
	}

	char* upgradeHeaderBuffer = NULL;
	formatString(&upgradeHeaderBuffer, return false;, "%s%c%s", "Upgrade", '\0', "WebSocket");

	header[0].key = upgradeHeaderBuffer;
	header[0].value = upgradeHeaderBuffer + strlen(upgradeHeaderBuffer) + 1;

	char* connectionHeaderBuffer = NULL;
	formatString(&connectionHeaderBuffer, return false;, "%s%c%s", "Connection", '\0', "Upgrade");

	header[1].key = connectionHeaderBuffer;
	header[1].value = connectionHeaderBuffer + strlen(connectionHeaderBuffer) + 1;

	int result = sendHTTPMessageToConnection(descriptor, HTTP_STATUS_UPGRADE_REQUIRED,
	                                         malloced_message, MIME_TYPE_TEXT, header, headerAmount,
	                                         CONNECTION_SEND_FLAGS_MALLOCED);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "Error while sending a response (in sendFailedHandshakeMessage)\n");
	}
	return -1;
}

NODISCARD static int sendFailedHandshakeMessage(const ConnectionDescriptor* const descriptor,
                                                const char* error_reason) {

	LOG_MESSAGE(LogLevelTrace, "Failed WS handshake: %s\n", error_reason);

	StringBuilder* message = string_builder_init();

	string_builder_append(message, return false;
	                      , "Error: The client handshake was invalid: %s", error_reason);

	char* malloced_message = string_builder_get_string(message);
	int result =
	    sendHTTPMessageToConnection(descriptor, HTTP_STATUS_BAD_REQUEST, malloced_message,
	                                MIME_TYPE_TEXT, NULL, 0, CONNECTION_SEND_FLAGS_MALLOCED);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "Error while sending a response (in sendFailedHandshakeMessage)\n");
	}
	return -1;
}

#define EXPECTED_WS_HEADER_SEC_KEY_LENGTH 16

NODISCARD static bool isValidSecKey(const char* key) {
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

static const char* const keyAcceptConstant = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static char* generateKeyAnswer(const char* secKey) {

	char* keyToHashBuffer = NULL;
	formatString(&keyToHashBuffer, return NULL;, "%s%s", secKey, keyAcceptConstant);

	uint8_t* sha1_hash = sha1(keyToHashBuffer);

	char* result = b64_encode(sha1_hash, SHA1_LEN);

	free(sha1_hash);
	free(keyToHashBuffer);

	return result;
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HANDSHAKE_HEADER_NONE = 0b0,
	HANDSHAKE_HEADER_HEADER_HOST = 0b1,
	HANDSHAKE_HEADER_HEADER_UPGRADE = 0b10,
	HANDSHAKE_HEADER_HEADER_CONNECTION = 0b100,
	HANDSHAKE_HEADER_HEADER_SEC_WEBSOCKET_KEY = 0b1000,
	HANDSHAKE_HEADER_HEADER_SEC_WEBSOCKET_VERSION = 0b10000,
	//
	HANDSHAKE_HEADER_HEADER_ALL_FOUND = 0b11111,
} NEEDED_HEADER_FOR_HANDSHAKE;

static const bool SEND_HTTP_UPGRADE_REQUIRED_STATUS_CODE = true;

int handleWSHandshake(const HttpRequest* const httpRequest,
                      const ConnectionDescriptor* const descriptor) {

	// check if it is a valid Websocket request
	// according to rfc https://datatracker.ietf.org/doc/html/rfc6455#section-2 section 4.2.1.
	NEEDED_HEADER_FOR_HANDSHAKE foundList = HANDSHAKE_HEADER_NONE;

	char* secKey = NULL;
	bool fromBrowser = false;

	for(size_t i = 0; i < httpRequest->head.headerAmount; ++i) {
		HttpHeaderField header = httpRequest->head.headerFields[i];
		if(strcasecmp(header.key, "host") == 0) {
			foundList |= HANDSHAKE_HEADER_HEADER_HOST;
		} else if(strcasecmp(header.key, "upgrade") == 0) {
			foundList |= HANDSHAKE_HEADER_HEADER_UPGRADE;
			if(strcasecontains(header.value, "websocket") < 0) {
				return sendFailedHandshakeMessage(descriptor,
				                                  "upgrade does not contain 'websocket'");
			}
		} else if(strcasecmp(header.key, "connection") == 0) {
			foundList |= HANDSHAKE_HEADER_HEADER_CONNECTION;
			if(strcasecontains(header.value, "upgrade") < 0) {
				if(SEND_HTTP_UPGRADE_REQUIRED_STATUS_CODE) {
					return sendFailedHandshakeMessageUpgradeRequired(descriptor);
				}

				return sendFailedHandshakeMessage(descriptor,
				                                  "connection does not contain 'upgrade'");
			}
		} else if(strcasecmp(header.key, "sec-websocket-key") == 0) {
			foundList |= HANDSHAKE_HEADER_HEADER_SEC_WEBSOCKET_KEY;
			if(isValidSecKey(header.value)) {
				secKey = header.value;
			} else {
				return sendFailedHandshakeMessage(descriptor, "sec-websocket-key is invalid");
			}
		} else if(strcasecmp(header.key, "sec-websocket-version") == 0) {
			foundList |= HANDSHAKE_HEADER_HEADER_SEC_WEBSOCKET_VERSION;
			if(strcmp(header.value, "13") != 0) {
				return sendFailedHandshakeMessage(descriptor,
				                                  "sec-websocket-version has invalid value");
			}
		} else if(strcasecmp(header.key, "origin") == 0) {
			fromBrowser = true;
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

	UNUSED(fromBrowser);

	if((HANDSHAKE_HEADER_HEADER_ALL_FOUND & foundList) != HANDSHAKE_HEADER_HEADER_ALL_FOUND) {
		if(SEND_HTTP_UPGRADE_REQUIRED_STATUS_CODE && /*NOLINT(readability-implicit-bool-conversion)*/
		   ((foundList & HANDSHAKE_HEADER_HEADER_UPGRADE) == 0)) {
			return sendFailedHandshakeMessageUpgradeRequired(descriptor);
		}
		return sendFailedHandshakeMessage(descriptor, "missing required headers");
	}

	// send server handshake

	const int headerAmount = 3;

	HttpHeaderField* header = (HttpHeaderField*)malloc(sizeof(HttpHeaderField) * headerAmount);

	if(!header) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return -1;
	}

	char* upgradeHeaderBuffer = NULL;
	formatString(&upgradeHeaderBuffer, return false;, "%s%c%s", "Upgrade", '\0', "WebSocket");

	header[0].key = upgradeHeaderBuffer;
	header[0].value = upgradeHeaderBuffer + strlen(upgradeHeaderBuffer) + 1;

	char* connectionHeaderBuffer = NULL;
	formatString(&connectionHeaderBuffer, return false;, "%s%c%s", "Connection", '\0', "Upgrade");

	header[1].key = connectionHeaderBuffer;
	header[1].value = connectionHeaderBuffer + strlen(connectionHeaderBuffer) + 1;

	char* keyAnswer = generateKeyAnswer(secKey);

	char* secWebsocketAcceptHeaderBuffer = NULL;
	formatString(&secWebsocketAcceptHeaderBuffer, return false;
	             , "%s%c%s", "Sec-WebSocket-Accept", '\0', keyAnswer);

	free(keyAnswer);

	header[2].key = secWebsocketAcceptHeaderBuffer;
	header[2].value = secWebsocketAcceptHeaderBuffer + strlen(secWebsocketAcceptHeaderBuffer) + 1;

	return sendHTTPMessageToConnection(descriptor, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, NULL,
	                                   header, headerAmount, CONNECTION_SEND_FLAGS_MALLOCED);
}
