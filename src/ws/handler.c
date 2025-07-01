
#include "./handler.h"
#include "utils/log.h"

WebSocketAction websocket_function(WebSocketConnection* connection, WebSocketMessage message) {

	if(message.is_text) {

		if(message.data_len >=
		   200) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message of length %" PRIu64 "\n",
			            message.data_len);
		} else {
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message: '%.*s'\n", (int)(message.data_len),
			            (char*)message.data);
		}
	} else {
		LOG_MESSAGE(LogLevelInfo, "Received BIN message of length %" PRIu64 "\n", message.data_len);
	}

	// for autobahn tests, just echoing the things
	int result = ws_send_message(connection, message);

	if(result) {
		return WebSocketActionError;
	}

	return WebSocketActionContinue;
}

WebSocketAction websocket_function_fragmented(WebSocketConnection* connection,
                                            WebSocketMessage message) {

	if(message.is_text) {

		if(message.data_len >=
		   200) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message of length %" PRIu64 "\n",
			            message.data_len);
		} else {
			LOG_MESSAGE(LogLevelInfo, "Received TEXT message: '%.*s'\n", (int)(message.data_len),
			            (char*)message.data);
		}
	} else {
		LOG_MESSAGE(LogLevelInfo, "Received BIN message of length %" PRIu64 "\n", message.data_len);
	}

	// for autobahn tests, just echoing the things
	int result = ws_send_message_fragmented(connection, message, WsFragmentationAuto);

	if(result < 0) {
		return WebSocketActionError;
	}

	return WebSocketActionContinue;
}
