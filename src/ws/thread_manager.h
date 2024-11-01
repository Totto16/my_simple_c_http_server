
#pragma once

#include "generic/secure.h"

typedef struct WebSocketThreadManagerImpl WebSocketThreadManager;

typedef struct WebSocketConnectionImpl WebSocketConnection;

typedef struct {
	bool is_text;
	void* data;
	uint64_t data_len;
} WebSocketMessage;

typedef bool (*WebSocketFunction)(WebSocketConnection* connection, WebSocketMessage* message);

void ws_send_message(WebSocketConnection* connection, WebSocketMessage message);

/**
 * NOT Thread safe
 */
WebSocketThreadManager* initialize_thread_manager();

/**
 * Thread safe
 */
WebSocketConnection* thread_manager_add_connection(WebSocketThreadManager* manager,
                                                   const ConnectionDescriptor* const descriptor,
                                                   ConnectionContext* context,
                                                   WebSocketFunction function);

/**
 * Thread safe
 *
 * returns true if it was successfully removed, false if it was an invalid connection
 */
bool thread_manager_remove_connection(WebSocketThreadManager* manager,
                                      WebSocketConnection* connection);

/**
 * NOT Thread safe
 */
void thread_manager_remove_all_connections(WebSocketThreadManager* manager);

/**
 * NOT Thread safe
 */
void free_thread_manager(WebSocketThreadManager* manager);
