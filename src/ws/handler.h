#pragma once

#include "./thread_manager.h"

NODISCARD WebSocketAction websocket_function(WebSocketConnection* connection,
                                             WebSocketMessage* message, WsConnectionArgs args,
                                             ExtensionSendState* extension_send_state);
