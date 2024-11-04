
#include "./handler.h"
#include "utils/log.h"

WebSocketAction websocketFunction(WebSocketConnection* connection, WebSocketMessage message) {

	if(message.is_text) {

		if(message.data_len >= 200) {
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message of length %lu\n", message.data_len);
		} else {
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message: '%.*s'\n", (int)(message.data_len),
			            (char*)message.data);
		}
	} else {
		LOG_MESSAGE(LogLevelInfo, "Received BIN message of length %lu\n", message.data_len);
	}

	// for autobahn tests, just echoing the things
	bool result = ws_send_message(connection, message);

	if(!result) {
		return WebSocketAction_Error;
	}

	return WebSocketAction_Continue;
}

WebSocketAction websocketFunctionFragmented(WebSocketConnection* connection,
                                            WebSocketMessage message) {

	if(message.is_text) {

		if(message.data_len >= 200) {
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message of length %lu\n", message.data_len);
		} else {
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message: '%.*s'\n", (int)(message.data_len),
			            (char*)message.data);
		}
	} else {
		LOG_MESSAGE(LogLevelInfo, "Received BIN message of length %lu\n", message.data_len);
	}

	// for autobahn tests, just echoing the things
	bool result = ws_send_message_fragmented(connection, message, WS_FRAGMENTATION_AUTO);

	if(!result) {
		return WebSocketAction_Error;
	}

	return WebSocketAction_Continue;
}
