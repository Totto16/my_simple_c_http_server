
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

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WebSocketActionContinue = 0,
	WebSocketActionError,
	WebSocketActionClose
} WebSocketAction;

typedef WebSocketAction (*WebSocketFunction)(WebSocketConnection* connection,
                                             WebSocketMessage message);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WsFragmentationOff,
	WsFragmentationAuto,
} FragmentOption;

#define WS_MINIMUM_FRAGMENT_SIZE 16

NODISCARD int ws_send_message(WebSocketConnection* connection, WebSocketMessage message);

NODISCARD int ws_send_message_fragmented(WebSocketConnection* connection, WebSocketMessage message,
                                         int64_t fragment_size);

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
                                                             WebSocketFunction function);

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
