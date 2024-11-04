
#pragma once

#include "generic/secure.h"
#include "utils/utils.h"

typedef struct WebSocketThreadManagerImpl WebSocketThreadManager;

typedef struct WebSocketConnectionImpl WebSocketConnection;

typedef struct {
	bool is_text;
	void* data;
	uint64_t data_len;
} WebSocketMessage;

typedef enum {
	WebSocketAction_Continue,
	WebSocketAction_Error,
	WebSocketAction_Close
} WebSocketAction;

typedef WebSocketAction (*WebSocketFunction)(WebSocketConnection* connection,
                                             WebSocketMessage message);

typedef enum { WS_FRAGMENTATION_OFF, WS_FRAGMENTATION_AUTO } FragmentOption;

#define WS_MINIMUM_FRAGMENT_SIZE 16

NODISCARD int ws_send_message(WebSocketConnection* connection, WebSocketMessage message);

NODISCARD int ws_send_message_fragmented(WebSocketConnection* connection, WebSocketMessage message,
                                         int64_t fragment_size);

/**
 * NOT Thread safe
 */
WebSocketThreadManager* initialize_thread_manager(void);

/**
 * Thread safe
 */
WebSocketConnection* thread_manager_add_connection(WebSocketThreadManager* manager,
                                                   const ConnectionDescriptor* descriptor,
                                                   ConnectionContext* context,
                                                   WebSocketFunction function);

/**
 * Thread safe
 *
 * returns true if it was successfully removed, false if it was an invalid connection
 */
NODISCARD bool thread_manager_remove_connection(WebSocketThreadManager* manager,
                                                WebSocketConnection* connection);

/**
 * NOT Thread safe
 */
NODISCARD bool thread_manager_remove_all_connections(WebSocketThreadManager* manager);

/**
 * NOT Thread safe
 */
NODISCARD bool free_thread_manager(WebSocketThreadManager* manager);
