

#include "thread_manager.h"
#include "generic/read.h"
#include "generic/send.h"
#include "utils/utils.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/random.h>

struct connection_node_t {
	WebSocketConnection* connection;
	struct connection_node_t* next;
};

struct WebSocketThreadManagerImpl {
	pthread_mutex_t mutex;
	struct connection_node_t* head;
};

struct WebSocketConnectionImpl {
	ConnectionContext* context;
	const ConnectionDescriptor* descriptor;
	WebSocketFunction function;
	pthread_t thread_id;
};

typedef enum {
	WS_OPCODE_CONT = 0x0,
	WS_OPCODE_TEXT = 0x1,
	WS_OPCODE_BIN = 0x2,
	// 0x3 - 0x7 are reserved for further non-control frames
	WS_OPCODE_CLOSE = 0x8,
	WS_OPCODE_PING = 0x9,
	WS_OPCODE_PONG = 0xA,
	// 0xB - 0xF are reserved for further control frames

} WS_OPCODE;

typedef struct {
	bool fin;
	WS_OPCODE opCode;
	bool mask;
	uint8_t payload_len;
} RawHeaderOne;

typedef struct {
	bool fin;
	WS_OPCODE opCode;
	void* payload;
	uint64_t payload_len;
} WebSocketRawMessage;

typedef struct {
	WebSocketConnection* connection;
	WebSocketThreadManager* manager;
} WebSocketListenerArg;

// TODO: free every readExactBytes result in the correct places

#define RAW_MESSAGE_HEADER_SIZE 2

static RawHeaderOne get_raw_header(uint8_t header_bytes[RAW_MESSAGE_HEADER_SIZE]) {
	bool fin = (header_bytes[0] >> 7) & 0b1;
	uint8_t rsv_bytes = (header_bytes[0] >> 4) & 0b111;
	// TODO: better error handling
	assert(rsv_bytes == 0 && "only 0 allowed for the rsv bytes");
	WS_OPCODE opCode = header_bytes[0] & 0b1111;

	bool mask = (header_bytes[1] >> 7) & 0b1;
	uint8_t payload_len = header_bytes[1] & 0x7F;

	RawHeaderOne result = {
		.fin = fin, .opCode = opCode, .mask = mask, .payload_len = payload_len
	};
	return result;
}

static WebSocketRawMessage read_raw_message(WebSocketConnection* connection) {

	uint8_t* header_bytes =
	    (uint8_t*)readExactBytes(connection->descriptor, RAW_MESSAGE_HEADER_SIZE);
	RawHeaderOne raw_header = get_raw_header(header_bytes);

	uint64_t payload_len = (uint64_t)raw_header.payload_len;

	if(payload_len == 126) {
		uint16_t* payload_len_result = (uint16_t*)readExactBytes(connection->descriptor, 2);
		payload_len = (uint64_t)(*payload_len_result);
	} else if(payload_len == 127) {
		uint64_t* payload_len_result = (uint64_t*)readExactBytes(connection->descriptor, 8);
		payload_len = *payload_len_result;
	}

	uint8_t* mask_byte = NULL;

	if(raw_header.mask) {
		mask_byte = (uint8_t*)readExactBytes(connection->descriptor, 4);
	}

	void* payload = readExactBytes(connection->descriptor, payload_len);

	if(raw_header.mask) {
		for(size_t i = 0; i < payload_len; ++i) {
			((uint8_t*)payload)[i] = ((uint8_t*)payload)[i] ^ mask_byte[i % 4];
		}
	}

	WebSocketRawMessage result = { .fin = raw_header.fin,
		                           .opCode = raw_header.opCode,
		                           .payload = payload,
		                           .payload_len = payload_len };

	return result;
}

void ws_send_message_raw_internal(WebSocketConnection* connection, WebSocketRawMessage raw_message,
                                  bool mask) {

	if(raw_message.payload == NULL) {
		assert(raw_message.payload_len == 0 && "payload and payload length have to match");
	}

	uint8_t mask_len = mask ? 4 : 0;

	uint8_t payload_additional_len =
	    raw_message.payload_len < 126 ? 0 : (raw_message.payload_len < 0x10000 ? 2 : 8);

	uint64_t header_offset = RAW_MESSAGE_HEADER_SIZE + payload_additional_len + mask_len;

	uint64_t size = header_offset + raw_message.payload_len;

	uint8_t* resultingFrame = (uint8_t*)mallocOrFail(size, false);

	uint8_t headerOne = ((raw_message.fin & 0b1) << 7) | (raw_message.opCode & 0b1111);

	uint8_t payload_len_1 = payload_additional_len == 0 ? raw_message.payload_len
	                                                    : (payload_additional_len == 2 ? 126 : 127);

	uint8_t headerTwo = ((mask & 0b1) << 7) | (payload_len_1 & 0x7F);

	resultingFrame[0] = headerOne;
	resultingFrame[1] = headerTwo;

	if(payload_additional_len == 2) {
		*((uint16_t*)(resultingFrame + 2)) = (uint16_t)(raw_message.payload_len);
	} else if(payload_additional_len == 8) {
		*((uint64_t*)(resultingFrame + 2)) = raw_message.payload_len;
	}

	if(raw_message.payload_len != 0) {
		memcpy(resultingFrame + header_offset, raw_message.payload, raw_message.payload_len);
	}

	if(mask) {

		uint32_t mask_byte = 0;
		getrandom((uint8_t*)(&mask_byte), sizeof(uint32_t), 0);

		*((uint32_t*)(resultingFrame + RAW_MESSAGE_HEADER_SIZE + payload_additional_len)) =
		    mask_byte;

		for(size_t i = 0; i < raw_message.payload_len; ++i) {
			((uint8_t*)resultingFrame + header_offset)[i] =
			    ((uint8_t*)raw_message.payload)[i] ^ ((uint8_t*)(&mask_byte))[i % 4];
		}
	}

	sendDataToConnection(connection->descriptor, resultingFrame, size);
}

void ws_send_message_internal(WebSocketConnection* connection, WebSocketMessage message,
                              bool mask) {
	WS_OPCODE opCode = message.is_text ? WS_OPCODE_TEXT : WS_OPCODE_BIN;

	WebSocketRawMessage raw_message = {
		.fin = true, .opCode = opCode, .payload = message.data, .payload_len = message.data_len
	};
	ws_send_message_raw_internal(connection, raw_message, mask);
}

typedef enum /* :uint16_t */ {
	CloseCode_Normal = 1000,
	CloseCode_GoingAway = 1001,
	CloseCode_ProtocolError = 1002,
	CloseCode_NotSupportedType = 1003,
	//
	CloseCode_MessageDataCorrupt = 1007,
	CloseCode_PolicyViolation = 1008,
	CloseCode_MessageToBig = 1009,
	CloseCode_MissingExtension = 1010,
	CloseCode_UnexpectedCondition = 1011
} CloseCode;

typedef struct {
	CloseCode code; // as uint16_t
	char* message;
} CloseReason;

static CloseReason maybe_parse_close_reason(WebSocketRawMessage raw_message,
                                            bool also_parse_message) {
	assert(raw_message.opCode == WS_OPCODE_CLOSE);

	uint64_t payload_len = raw_message.payload_len;

	if(payload_len < 2) {
		CloseReason failed = { .code = 0, .message = NULL };
		return failed;
	}

	uint8_t* message = (uint8_t*)raw_message.payload;

	uint16_t code = 0;

	((uint8_t*)(&code))[0] = message[0];
	((uint8_t*)(&code))[1] = message[1];

	if(payload_len > 2 && also_parse_message) {
		CloseReason result = { .code = code, .message = (char*)(message + 2) };
		return result;
	}

	CloseReason result = { .code = code, .message = "" };
	return result;
}

static void ws_send_close_message_raw_internal(WebSocketConnection* connection,
                                               CloseReason reason) {

	size_t message_len = strlen(reason.message);

	uint64_t payload_len = 2 + message_len;

	uint8_t* payload = (uint8_t*)mallocOrFail(payload_len, false);

	uint8_t* reason_code = (uint8_t*)(&reason.code);

	payload[0] = reason_code[0];
	payload[1] = reason_code[1];

	memcpy(payload + 2, reason.message, message_len);

	WebSocketRawMessage message_raw = {
		.fin = true, .opCode = WS_OPCODE_CLOSE, .payload = payload, .payload_len = payload_len
	};

	ws_send_message_raw_internal(connection, message_raw, false);
}

static void close_websocket_connection(WebSocketConnection* connection,
                                       WebSocketThreadManager* manager, CloseReason reason) {
	ws_send_close_message_raw_internal(connection, reason);

	thread_manager_remove_connection(manager, connection);
}

void* wsListenerFunction(anyType(WebSocketListenerArg*) arg) {

	// TODO: free in every possible path;
	WebSocketListenerArg* argument = (WebSocketListenerArg*)arg;

	WebSocketConnection* connection = argument->connection;

	while(true) {
		bool has_message = false;
		WebSocketMessage current_message = { .is_text = true, .data = NULL, .data_len = 0 };

		while(true) {

			WebSocketRawMessage raw_message = read_raw_message(connection);

			switch(raw_message.opCode) {
				case WS_OPCODE_CONT: {
					if(!has_message) {
						CloseReason reason = {
							.code = CloseCode_ProtocolError,
							"Received Opcode CONTINUATION, but no start frame received"
						};

						close_websocket_connection(connection, argument->manager, reason);
						return NULL;
					}

					uint64_t old_length = current_message.data_len;
					void* old_data = current_message.data;

					current_message.data =
					    mallocOrFail(old_length + raw_message.payload_len, false);
					current_message.data_len += raw_message.payload_len;

					memcpy(current_message.data, old_data, old_length);
					memcpy(((uint8_t*)current_message.data) + old_length, raw_message.payload,
					       raw_message.payload_len);

					free(old_data);

					if(!raw_message.fin) {
						continue;
					}

					// can't break out of a switch and the while loop, so using goto
					goto handle_message;
				}

				case WS_OPCODE_TEXT:
				case WS_OPCODE_BIN: {
					has_message = true;

					current_message.is_text = raw_message.opCode == WS_OPCODE_TEXT;
					current_message.data = raw_message.payload;
					current_message.data_len = raw_message.payload_len;

					if(!raw_message.fin) {
						continue;
					}

					// can't break out of a switch and the while loop, so using goto
					goto handle_message;
				}
				case WS_OPCODE_CLOSE: {

					CloseReason reason = { .code = CloseCode_Normal, "Planned close" };

					if(raw_message.payload_len != 0) {
						CloseReason new_reason = maybe_parse_close_reason(raw_message, false);
						if(new_reason.code != 0) {
							reason = new_reason;
						}
					}

					close_websocket_connection(connection, argument->manager, reason);
					return NULL;
				}

				case WS_OPCODE_PING: {
					WebSocketRawMessage message_raw = { .fin = true,
						                                .opCode = WS_OPCODE_PONG,
						                                .payload = raw_message.payload,
						                                .payload_len = raw_message.payload_len };
					ws_send_message_raw_internal(connection, message_raw, false);
					continue;
				}

				case WS_OPCODE_PONG: {
					// just ignore
					continue;
				}

				default: {
					CloseReason reason = { .code = CloseCode_NotSupportedType,
						                   "Received Opcode that is not supported" };

					close_websocket_connection(connection, argument->manager, reason);
					return NULL;
				}
			}
		}

	handle_message:

		if(has_message) {
			bool everything_ok = connection->function(connection, current_message);
			free(current_message.data);

			if(!everything_ok) {
				CloseReason reason = { .code = CloseCode_Normal,
					                   "ServerApplication requested shutdown" };

				close_websocket_connection(connection, argument->manager, reason);
				return NULL;
			}
		}
	}

	return NULL;
}

void ws_send_message(WebSocketConnection* connection, WebSocketMessage message) {

	ws_send_message_internal(connection, message, false);
}

WebSocketThreadManager* initialize_thread_manager() {

	WebSocketThreadManager* manager = mallocOrFail(sizeof(WebSocketThreadManager), true);

	int result = pthread_mutex_init(&manager->mutex, NULL);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to initialize the mutex for the WebSocketThreadManager");
	manager->head = NULL;

	return manager;
}

WebSocketConnection* thread_manager_add_connection(WebSocketThreadManager* manager,
                                                   const ConnectionDescriptor* const descriptor,
                                                   ConnectionContext* context,
                                                   WebSocketFunction function) {

	int result = pthread_mutex_lock(&manager->mutex);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to lock the mutex for the WebSocketThreadManager");

	WebSocketConnection* connection = mallocOrFail(sizeof(WebSocketConnection), true);

	connection->context = context;
	connection->descriptor = descriptor;
	connection->function = function;

	struct connection_node_t* current_node = manager->head;

	while(true) {
		if(current_node == NULL) {
			struct connection_node_t* new_node =
			    mallocOrFail(sizeof(struct connection_node_t), true);

			new_node->connection = connection;
			new_node->next = NULL;
			break;
		}

		current_node = current_node->next;
	}

	WebSocketListenerArg* threadArgument =
	    (WebSocketListenerArg*)mallocOrFail(sizeof(WebSocketListenerArg), true);
	// initializing the struct with the necessary values
	threadArgument->connection = connection;
	threadArgument->manager = manager;

	result = pthread_create(&connection->thread_id, NULL, wsListenerFunction, threadArgument);
	checkResultForThreadErrorAndExit("An Error occurred while trying to create a new Thread");

	result = pthread_mutex_unlock(&manager->mutex);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to unlock the mutex for the WebSocketThreadManager");

	return connection;
}

static void free_connection(WebSocketConnection* connection, bool send_go_away) {

	if(send_go_away) {
		CloseReason reason = { .code = CloseCode_GoingAway, "Server is shutting down" };
		ws_send_close_message_raw_internal(connection, reason);
	}

	close_connection_descriptor(connection->descriptor, connection->context);
	free_connection_context(connection->context);
	free(connection);
}

bool thread_manager_remove_connection(WebSocketThreadManager* manager,
                                      WebSocketConnection* connection) {

	if(connection == NULL) {
		return false;
	}

	int result = pthread_mutex_lock(&manager->mutex);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to lock the mutex for the WebSocketThreadManager");

	struct connection_node_t* current_node = manager->head;
	struct connection_node_t* previous_node = NULL;

	bool return_value = false;

	while(true) {
		if(current_node == NULL) {
			return_value = false;
			break;
		}

		// TODO: shut down connection, if it is still running e.g. in the case of GET to /shutdown

		WebSocketConnection* this_connection = current_node->connection;
		if(connection == this_connection) {
			free_connection(connection, false);

			struct connection_node_t* next_node = current_node->next;

			if(previous_node == NULL) {
				manager->head = next_node;
			} else {
				previous_node->next = next_node;
			}

			free(current_node);
			return_value = true;
			break;
		}

		previous_node = current_node;
		current_node = current_node->next;
	}

	result = pthread_mutex_unlock(&manager->mutex);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to unlock the mutex for the WebSocketThreadManager");

	return return_value;
}

void thread_manager_remove_all_connections(WebSocketThreadManager* manager) {

	struct connection_node_t* current_node = manager->head;

	while(true) {
		if(current_node == NULL) {
			break;
		}

		// TODO: shut down connections, if they are still running e.g. in the case of GET to
		// /shutdown

		WebSocketConnection* connection = current_node->connection;

		int result = pthread_cancel(connection->thread_id);
		checkResultForErrorAndExit("While trying to cancel a WebSocketConnection Thread");

		free_connection(connection, true);

		struct connection_node_t* to_free = current_node;

		current_node = current_node->next;
		free(to_free);
	}
}

void free_thread_manager(WebSocketThreadManager* manager) {

	int result = pthread_mutex_destroy(&manager->mutex);
	checkResultForThreadErrorAndExit("An Error occurred while trying to destroy the mutex in "
	                                 "cleaning up for the WebSocketThreadManager");

	assert(manager->head == NULL && "All connections got removed correctly");

	free(manager);
}
