#pragma once

#include "./thread_manager.h"

WebSocketAction websocketFunction(WebSocketConnection* connection, WebSocketMessage message);

WebSocketAction websocketFunctionFragmented(WebSocketConnection* connection,
                                            WebSocketMessage message);
