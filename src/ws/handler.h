#pragma once

#include "./thread_manager.h"

NODISCARD WebSocketAction websocket_function(WebSocketConnection* connection, WebSocketMessage message);

NODISCARD WebSocketAction websocket_function_fragmented(WebSocketConnection* connection,
                                              WebSocketMessage message);
