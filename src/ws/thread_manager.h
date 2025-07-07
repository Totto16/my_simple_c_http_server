
#pragma once

#include "./types.h"
#include "./ws.h"
#include "generic/secure.h"
#include "utils/utils.h"

typedef struct WebSocketThreadManagerImpl WebSocketThreadManager;

typedef struct WebSocketConnectionImpl WebSocketConnection;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WebSocketActionContinue = 0,
	WebSocketActionError,
	WebSocketActionClose
} WebSocketAction;

typedef WebSocketAction (*WebSocketFunction)(WebSocketConnection* connection,
                                             WebSocketMessage* message, WsConnectionArgs args,
                                             ExtensionSendState* extension_send_state);

NODISCARD int ws_send_message(WebSocketConnection* connection, WebSocketMessage* message,
                              WsConnectionArgs args, ExtensionSendState* extension_send_state);

void free_ws_message(WebSocketMessage message);

void free_raw_ws_message(WebSocketRawMessage message);

/**
 * NOT Thread safe
 */
NODISCARD WebSocketThreadManager* initialize_thread_manager(void);

/**
 * Thread safe
 */
NODISCARD WebSocketConnection* thread_manager_add_connection(WebSocketThreadManager* manager,
                                                             ConnectionDescriptor* descriptor,
                                                             ConnectionContext* context,
                                                             WebSocketFunction function,
                                                             WsConnectionArgs args);

/**
 * Thread safe
 *
 * returns true if it was successfully removed, false if it was an invalid connection
 */
NODISCARD int thread_manager_remove_connection(WebSocketThreadManager* manager,
                                               WebSocketConnection* connection);

/**
 * NOT Thread safe
 */
NODISCARD bool thread_manager_remove_all_connections(WebSocketThreadManager* manager);

/**
 * NOT Thread safe
 */
NODISCARD bool free_thread_manager(WebSocketThreadManager* manager);
