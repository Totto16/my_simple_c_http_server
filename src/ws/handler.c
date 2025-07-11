
#include "./handler.h"
#include "utils/log.h"

#define MAX_PRINT_FOR_TEXT_MESSAGE 200

WebSocketAction websocket_function(WebSocketConnection* connection, WebSocketMessage* message,
                                   WsConnectionArgs args,
                                   ExtensionSendState* extension_send_state) {

	if(args.trace) {
		if(message->is_text) {

			if(message->data_len >= MAX_PRINT_FOR_TEXT_MESSAGE) {
				LOG_MESSAGE(LogLevelInfo, "Received TEXT message of length %" PRIu64 "\n",
				            message->data_len);
			} else {
				LOG_MESSAGE(LogLevelInfo, "Received TEXT message: '%.*s'\n",
				            (int)(message->data_len), (char*)message->data);
			}
		} else {
			LOG_MESSAGE(LogLevelInfo, "Received BIN message of length %" PRIu64 "\n",
			            message->data_len);
		}
	}

	// for autobahn tests, just echoing the things
	int result = ws_send_message(connection, message, args, extension_send_state);

	if(result < 0) {
		return WebSocketActionError;
	}

	return WebSocketActionContinue;
}
