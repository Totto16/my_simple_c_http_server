

#include "thread_manager.h"
#include "generic/read.h"
#include "generic/send.h"
#include "utils/log.h"
#include "utils/thread_helper.h"
#include "utils/utils.h"

#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/random.h>
#include <utf8proc.h>

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
	bool has_error;
	union {
		RawHeaderOne header;
		const char* error;
	} data;
} RawHeaderOneResult;

typedef struct {
	bool fin;
	WS_OPCODE opCode;
	void* payload;
	uint64_t payload_len;
} WebSocketRawMessage;

typedef struct {
	bool has_error;
	union {
		WebSocketRawMessage message;
		const char* error;
	} data;
} WebSocketRawMessageResult;

typedef struct {
	WebSocketConnection* connection;
	WebSocketThreadManager* manager;
} WebSocketListenerArg;

#define RAW_MESSAGE_HEADER_SIZE 2

#define RAW_MESSAGE_PAYLOAD_1_SIZE 2

#define RAW_MESSAGE_PAYLOAD_2_SIZE 8

#define RAW_MESSAGE_MASK_BYTE_SIZE 4

#define WS_MAXIMUM_HEADER_LENGTH \
	(RAW_MESSAGE_HEADER_SIZE + RAW_MESSAGE_PAYLOAD_2_SIZE + RAW_MESSAGE_MASK_BYTE_SIZE)

#define MAX_CONTROL_FRAME_PAYLOAD 125
#define EXTENDED_PAYLOAD_MAGIC_NUMBER1 126
#define EXTENDED_PAYLOAD_MAGIC_NUMBER2 127

NODISCARD static RawHeaderOneResult
get_raw_header(uint8_t const header_bytes[RAW_MESSAGE_HEADER_SIZE]) {
	bool fin = (header_bytes[0] >> // NOLINT(readability-implicit-bool-conversion)
	            7 // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	            ) &
	           0b1; // NOLINT(readability-implicit-bool-conversion)
	uint8_t rsv_bytes =
	    (header_bytes[0] >> 4) &
	    0b111; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	if(rsv_bytes != 0) {
		return (RawHeaderOneResult){ .has_error = true,
			                         .data = { .error = "only 0 allowed for the rsv bytes" } };
	};

	WS_OPCODE opCode =
	    header_bytes[0] &
	    0b1111; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	bool mask = (header_bytes[1] >> // NOLINT(readability-implicit-bool-conversion)
	             7) & // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	            0b1;  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t payload_len =
	    header_bytes[1] &
	    0x7F; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	RawHeaderOne header = {
		.fin = fin, .opCode = opCode, .mask = mask, .payload_len = payload_len
	};

	RawHeaderOneResult result = { .has_error = false, .data = { .header = header } };
	return result;
}

NODISCARD static bool is_control_op_code(WS_OPCODE opCode) {
	// wrong opcodes, they only can be 4 bytes large
	if(opCode > 0xF) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		return false;
	}

	return (opCode &   // NOLINT(readability-implicit-bool-conversion)
	        0b1000) != // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	       0;
}

NODISCARD static WebSocketRawMessageResult read_raw_message(WebSocketConnection* connection) {

	uint8_t* header_bytes =
	    (uint8_t*)readExactBytes(connection->descriptor, RAW_MESSAGE_HEADER_SIZE);
	if(!header_bytes) {
		return (WebSocketRawMessageResult){ .has_error = true,
			                                .data = { .error = "couldn't read header bytes (2)" } };
	}
	RawHeaderOneResult raw_header_result = get_raw_header(header_bytes);

	free(header_bytes);

	if(raw_header_result.has_error) {
		return (WebSocketRawMessageResult){ .has_error = true,
			                                .data = { .error = raw_header_result.data.error } };
	}

	RawHeaderOne raw_header = raw_header_result.data.header;

	uint64_t payload_len = (uint64_t)raw_header.payload_len;

	if(payload_len == EXTENDED_PAYLOAD_MAGIC_NUMBER1) {
		uint16_t* payload_len_result =
		    (uint16_t*)readExactBytes(connection->descriptor, RAW_MESSAGE_PAYLOAD_1_SIZE);
		if(!payload_len_result) {
			return (WebSocketRawMessageResult){
				.has_error = true,
				.data = { .error = "couldn't read extended payload length bytes (2)" }
			};
		}
		// in network byte order
		payload_len = (uint64_t)htons(*payload_len_result);
		free(payload_len_result);
	} else if(payload_len == EXTENDED_PAYLOAD_MAGIC_NUMBER2) {
		uint64_t* payload_len_result =
		    (uint64_t*)readExactBytes(connection->descriptor, RAW_MESSAGE_PAYLOAD_2_SIZE);
		if(!payload_len_result) {
			return (WebSocketRawMessageResult){
				.has_error = true,
				.data = { .error = "couldn't read extended payload length bytes (8)" }
			};
		}
		// in network byte order (alias big endian = be)
		payload_len = htobe64(*payload_len_result);
		free(payload_len_result);
	}

	uint8_t* mask_byte = NULL;

	if(raw_header.mask) {
		mask_byte = (uint8_t*)readExactBytes(connection->descriptor, RAW_MESSAGE_MASK_BYTE_SIZE);
		if(!mask_byte) {
			return (WebSocketRawMessageResult){
				.has_error = true, .data = { .error = "couldn't read mask bytes (4)" }
			};
		}
	}

	void* payload = NULL;

	if(payload_len != 0) {

		payload = readExactBytes(connection->descriptor, payload_len);
		if(!payload) {
			return (WebSocketRawMessageResult){
				.has_error = true, .data = { .error = "couldn't read payload bytes" }
			};
		}
	}

	if(raw_header.mask) {
		for(size_t i = 0; i < payload_len; ++i) {
			((uint8_t*)payload)[i] = ((uint8_t*)payload)[i] ^ mask_byte[i % 4];
		}
		free(mask_byte);
	}

	WebSocketRawMessage value = { .fin = raw_header.fin,
		                          .opCode = raw_header.opCode,
		                          .payload = payload,
		                          .payload_len = payload_len };

	WebSocketRawMessageResult result = { .has_error = false, .data = { .message = value } };

	return result;
}

static NODISCARD int ws_send_message_raw_internal(WebSocketConnection* connection,
                                                  WebSocketRawMessage raw_message, bool mask) {

	if(raw_message.payload == NULL) {
		if(raw_message.payload_len != 0) {

			LOG_MESSAGE_SIMPLE(LogLevelWarn, "payload and payload length have to match\n");
			return -1;
		}
	}

	uint8_t mask_len = mask ? 4 : 0; // NOLINT(readability-implicit-bool-conversion)

	uint8_t payload_additional_len =
	    raw_message.payload_len < EXTENDED_PAYLOAD_MAGIC_NUMBER1
	        ? 0
	        : (raw_message.payload_len < // NOLINT(readability-avoid-nested-conditional-operator)
	                   0x10000 // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	               ? 2
	               : 8); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	uint64_t header_offset = RAW_MESSAGE_HEADER_SIZE + payload_additional_len + mask_len;

	uint64_t size = header_offset + raw_message.payload_len;

	uint8_t* resultingFrame = (uint8_t*)malloc(size);

	if(resultingFrame == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return -1;
	}

	uint8_t headerOne =
	    ((raw_message.fin & 0b1) // NOLINT(readability-implicit-bool-conversion)
	     << 7) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	    | (raw_message.opCode &
	       0b1111); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	uint8_t additional_payload_len_2 = payload_additional_len == 2 ? EXTENDED_PAYLOAD_MAGIC_NUMBER1
	                                                               : EXTENDED_PAYLOAD_MAGIC_NUMBER2;

	uint8_t payload_len_1 =
	    payload_additional_len == 0 ? raw_message.payload_len : additional_payload_len_2;

	uint8_t headerTwo =
	    ((mask & 0b1) // NOLINT(readability-implicit-bool-conversion)
	     << 7) |      // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	    (payload_len_1 &
	     0x7F); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	resultingFrame[0] = headerOne;
	resultingFrame[1] = headerTwo;

	if(payload_additional_len == 2) {
		// in network byte order
		*((uint16_t*)(resultingFrame + 2)) = htons((uint16_t)(raw_message.payload_len));
	} else if(payload_additional_len ==
	          8) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		// in network byte order (alias big endian = be)
		*((uint64_t*)(resultingFrame + 2)) = htobe64(raw_message.payload_len);
	}

	if(raw_message.payload_len != 0) {
		memcpy(resultingFrame + header_offset, raw_message.payload, raw_message.payload_len);
	}

	if(mask) {

		uint32_t mask_byte = 0;
		ssize_t result = getrandom((uint8_t*)(&mask_byte), sizeof(uint32_t), 0);

		if(result != sizeof(uint32_t)) {
			if(result < 0) {
				LOG_MESSAGE(LogLevelWarn, "Get random failed: %s\n", strerror(errno));
			}

			unsigned int seed = time(NULL);

			// use rand_r like normal rand:
			mask_byte = rand_r(&seed);
		}

		*((uint32_t*)(resultingFrame + RAW_MESSAGE_HEADER_SIZE + payload_additional_len)) =
		    mask_byte;

		for(size_t i = 0; i < raw_message.payload_len; ++i) {
			(resultingFrame + header_offset)[i] =
			    ((uint8_t*)raw_message.payload)[i] ^ ((uint8_t*)(&mask_byte))[i % 4];
		}
	}

	if(is_control_op_code(raw_message.opCode)) {
		if(!raw_message.fin) {
			// TODO(Totto): add error message
			LOG_MESSAGE_SIMPLE(LogLevelDebug,
			                   "Control frame payload is fragmented, that isn't allowed\n");

			free(resultingFrame);
			return -2;
		}

		if(raw_message.payload_len > MAX_CONTROL_FRAME_PAYLOAD) {
			// TODO(Totto): add error message
			LOG_MESSAGE(LogLevelDebug, "Control frame payload length is too large: %lu > %d\n",
			            raw_message.payload_len, MAX_CONTROL_FRAME_PAYLOAD);

			free(resultingFrame);
			return -3;
		}
	}

	int result = sendDataToConnection(connection->descriptor, resultingFrame, size);

	free(resultingFrame);

	return result;
}

static NODISCARD int ws_send_message_internal_normal(WebSocketConnection* connection,
                                                     WebSocketMessage message, bool mask) {

	WS_OPCODE opCode = message.is_text // NOLINT(readability-implicit-bool-conversion)
	                       ? WS_OPCODE_TEXT
	                       : WS_OPCODE_BIN;

	WebSocketRawMessage raw_message = {
		.fin = true, .opCode = opCode, .payload = message.data, .payload_len = message.data_len
	};
	return ws_send_message_raw_internal(connection, raw_message, mask);
}

static NODISCARD int ws_send_message_internal_fragmented(WebSocketConnection* connection,
                                                         WebSocketMessage message, bool mask,
                                                         uint64_t fragment_size) {

	// this is the minimum we set, so that everything (header + eventual mask) can be sent
	if(fragment_size < WS_MINIMUM_FRAGMENT_SIZE) {
		fragment_size = WS_MINIMUM_FRAGMENT_SIZE;
	}

	if(message.data_len < fragment_size) {
		return ws_send_message_internal_normal(connection, message, mask);
	}

	for(uint64_t start = 0; start < message.data_len; start += fragment_size) {
		uint64_t end = start + fragment_size;
		bool fin = false;
		uint64_t payload_len = fragment_size;

		if(end >= message.data_len) {
			end = message.data_len;
			fin = true;
			payload_len = end - start;
		}

		WS_OPCODE opCode =
		    start == 0
		        ? (message.is_text // NOLINT(readability-implicit-bool-conversion,readability-avoid-nested-conditional-operator)
		               ? WS_OPCODE_TEXT
		               : WS_OPCODE_BIN)
		        : WS_OPCODE_CONT;

		void* payload = ((uint8_t*)message.data) + start;

		WebSocketRawMessage raw_message = {
			.fin = fin, .opCode = opCode, .payload = payload, .payload_len = payload_len
		};
		int result = ws_send_message_raw_internal(connection, raw_message, mask);

		if(result < 0) {
			return result;
		}
	}

	return 0;
}

#define DEFAULT_AUTO_FRAGMENT_SIZE 4096

static NODISCARD int ws_send_message_internal(WebSocketConnection* connection,
                                              WebSocketMessage message, bool mask,
                                              int64_t fragment_size) {

	if(fragment_size == WS_FRAGMENTATION_OFF) {
		return ws_send_message_internal_normal(connection, message, mask);
	}

	if(fragment_size == WS_FRAGMENTATION_AUTO || fragment_size <= 0) {

		int socket_fd = get_underlying_socket(connection->descriptor);

		// the default value, if an error would occur
		uint64_t chosen_fragment_size = DEFAULT_AUTO_FRAGMENT_SIZE - WS_MAXIMUM_HEADER_LENGTH;

		if(socket_fd >= 0) {
			int buffer_size = 0;
			socklen_t buffer_size_len = sizeof(buffer_size);
			int result =
			    getsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, &buffer_size_len);

			if(result == 0) {
				if(buffer_size >= WS_MAXIMUM_HEADER_LENGTH) {
					// NOTE: this value is the doubled, if you set it, but we use th doubled value
					// here anyway
					// we subtract the header length, so that it is can fit into one buffer
					chosen_fragment_size = ((uint64_t)buffer_size) - WS_MAXIMUM_HEADER_LENGTH;
				}
			} else {
				LOG_MESSAGE(LogLevelWarn,
				            "Couldn't get sockopt SO_SNDBUF, using default value: %s\n",
				            strerror(errno));
			}
		}

		return ws_send_message_internal_fragmented(connection, message, mask, chosen_fragment_size);
	}

	return ws_send_message_internal_fragmented(connection, message, mask, (uint64_t)fragment_size);
}

// see: https://datatracker.ietf.org/doc/html/rfc6455#section-11.7
typedef enum /* :uint16_t */ {
	CloseCode_NormalClosure = 1000,
	CloseCode_GoingAway = 1001,
	CloseCode_ProtocolError = 1002,
	CloseCode_UnsupportedData = 1003,
	//
	CloseCode_InvalidFramePayloadData = 1007,
	CloseCode_PolicyViolation = 1008,
	CloseCode_MessageTooBig = 1009,
	CloseCode_MandatoryExtension = 1010,
	CloseCode_InternalServerError = 1011
} CloseCode;

typedef struct {
	CloseCode code; // as uint16_t
	char* message;
	int16_t message_len; // max length is 123 bytes
} CloseReason;

typedef struct {
	bool success;
	CloseReason reason;
} CloseReasonResult;

static NODISCARD CloseReasonResult maybe_parse_close_reason(WebSocketRawMessage raw_message,
                                                            bool also_parse_message) {
	if(raw_message.opCode != WS_OPCODE_CLOSE) {
		return (CloseReasonResult){
			.success = false,
			.reason = { .code = 0, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			            .message = NULL,
			            .message_len = 0 }
		};
	}

	uint64_t payload_len = raw_message.payload_len;

	if(payload_len < 2) {
		return (CloseReasonResult){
			.success = false,
			.reason = { .code = 0, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			            .message = NULL,
			            .message_len = 0 }
		};
	}

	uint8_t* message = (uint8_t*)raw_message.payload;

	uint16_t code = 0;

	// in network byte order
	((uint8_t*)(&code))[0] = message[1];
	((uint8_t*)(&code))[1] = message[0];

	if(payload_len > MAX_CONTROL_FRAME_PAYLOAD) {
		return (CloseReasonResult){
			.success = false,
			.reason = { .code = 0, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			            .message = NULL,
			            .message_len = 0 }
		};
	}

	if(payload_len > 2 && also_parse_message) { // NOLINT(readability-implicit-bool-conversion)
		return (CloseReasonResult){
			.success = true,
			.reason = { .code = code, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			            .message = (char*)(message + 2),
			            .message_len = (int16_t)(payload_len - 2) }
		};
	}

	return (CloseReasonResult){
		.success = true,
		.reason = { .code = code, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
		            .message = NULL,
		            .message_len = 0 }

	};
}

// see: https://datatracker.ietf.org/doc/html/rfc6455#section-7.4.2
// and https://datatracker.ietf.org/doc/html/rfc6455#section-7.4.1
static NODISCARD bool is_valid_close_code(uint16_t close_code) {
	if(close_code <=
	   999) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		return false;
	}

	if(close_code >=
	       3000 && // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	   close_code <=
	       4999) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		return true;
	}

	if(close_code >=
	       1000 && // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	   close_code <=
	       2999) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		if(close_code >=
		       1004 && // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		   close_code <=
		       1006) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			return false;
		}

		if(close_code >=
		   1015) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			return false;
		}

		return true;
	}

	// not specified, but also not valid
	return false;
}

static NODISCARD int ws_send_close_message_raw_internal(WebSocketConnection* connection,
                                                        CloseReason reason) {

	size_t reason_msg_len =
	    (reason.message_len < 0 ? strlen(reason.message) : (size_t)reason.message_len);

	size_t message_len = reason.message ? reason_msg_len : 0;

	uint64_t payload_len = 2 + message_len;

	uint8_t* payload = (uint8_t*)malloc(payload_len);

	if(payload == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return -1;
	}

	uint8_t* reason_code = (uint8_t*)(&reason.code);

	// network byte order
	payload[0] = reason_code[1];
	payload[1] = reason_code[0];

	if(reason.message) {
		memcpy(payload + 2, reason.message, message_len);
	}

	WebSocketRawMessage message_raw = {
		.fin = true, .opCode = WS_OPCODE_CLOSE, .payload = payload, .payload_len = payload_len
	};

	int result = ws_send_message_raw_internal(connection, message_raw, false);

	free(payload);

	return result;
}

static NODISCARD const char* close_websocket_connection(WebSocketConnection* connection,
                                                        WebSocketThreadManager* manager,
                                                        CloseReason reason) {

	if(reason.message) {
		int message_size =
		    reason.message_len < 0 ? (int)strlen(reason.message) : reason.message_len;
		LOG_MESSAGE(LogLevelTrace, "Closing the websocket connection: %.*s\n", message_size,
		            reason.message);
	} else {
		LOG_MESSAGE_SIMPLE(LogLevelTrace, "Closing the websocket connection: (no message)\n");
	}

	int result = ws_send_close_message_raw_internal(connection, reason);

	// even if above failed, we need to remove the connection nevertheless
	int result2 = thread_manager_remove_connection(manager, connection);

	if(result < 0) {
		return "send error";
	}

	if(result2 < 0) {
		return "thread manager remove error";
	}

	return NULL;
}

static NODISCARD bool setup_signal_handler(void) {

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = SIG_IGN;
	// initialize the mask to be empty
	int emptySetResult = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGPIPE);
	int result1 = sigaction(SIGPIPE, &action, NULL);
	if(result1 < 0 || emptySetResult < 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't set signal interception: %s\n", strerror(errno));
		return false;
	}

	return true;
}

typedef struct {
	utf8proc_int32_t* data;
	uint64_t size;
} Utf8Data;

typedef struct {
	bool has_error;
	union {
		Utf8Data result;
		const char* error;
	} data;
} Utf8DataResult;

// TODO(Totto): at the moment we adhere to the RFC, by only checking TEXT as a whole after we got
// all fragments, but the autobahn test suggest, that we may fail fast, if the reason for an utf-8
// error isn't missing bytes at the end of the payload, like e.g. if we receive a whole invalid
// sequence
NODISCARD Utf8DataResult get_utf8_string(const void* data, long size) {

	utf8proc_int32_t* buffer = malloc(sizeof(utf8proc_int32_t) * size);

	if(!buffer) {
		return (Utf8DataResult){ .has_error = true, .data = { .error = "failed malloc" } };
	}

	utf8proc_ssize_t result = utf8proc_decompose(
	    data, size, buffer, size,
	    0); // NOLINT(cppcoreguidelines-narrowing-conversions,clang-analyzer-optin.core.EnumCastOutOfRange)

	if(result < 0) {
		free(buffer);
		return (Utf8DataResult){ .has_error = true, .data = { .error = utf8proc_errmsg(result) } };
	}

	if(result != size) {
		// truncate the buffer
		void* new_buffer = realloc(buffer, sizeof(utf8proc_int32_t) * result);

		if(!new_buffer) {
			free(buffer);
			return (Utf8DataResult){ .has_error = true, .data = { .error = "failed realloc" } };
		}
		buffer = new_buffer;
	}

	Utf8Data utf8_data = { .size = result, .data = buffer };

	return (Utf8DataResult){ .has_error = false, .data = { .result = utf8_data } };
}

anyType(NULL) ws_listener_function(anyType(WebSocketListenerArg*) _arg) {

	WebSocketListenerArg* argument = (WebSocketListenerArg*)_arg;

	char* thread_name_buffer = NULL;
	// TODO(Totto): better report error
	formatString(&thread_name_buffer, return NULL;, "ws listener %d", get_thread_id());
	set_thread_name(thread_name_buffer);

	bool _result = setup_signal_handler();

	// an error message was already sent, and just because the setting of the signal handler failed,
	// we shouldn't exit or close the connection!
	UNUSED(_result);

#define FREE_AT_END() \
	do { \
		free(thread_name_buffer); \
		free(argument); \
	} while(false)

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting WS Listener\n");

	WebSocketConnection* connection = argument->connection;

	while(true) {
		bool has_message = false;
		WebSocketMessage current_message = { .is_text = true, .data = NULL, .data_len = 0 };

		while(true) {

			WebSocketRawMessageResult raw_message_result = read_raw_message(connection);

#define FREE_RAW_WS_MESSAGE() free(raw_message_result.data.message.payload)

			if(raw_message_result.has_error) {

				char* errorMessage = NULL;
				formatString(
				    &errorMessage,
				    {
					    FREE_AT_END();
					    return NULL;
				    },
				    "Error while reading the needed bytes for a frame: %s",
				    raw_message_result.data.error);

				LOG_MESSAGE(LogLevelInfo, "%s\n", errorMessage);

				CloseReason reason = { .code = CloseCode_ProtocolError,
					                   .message = errorMessage,
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(connection, argument->manager, reason);

				free(errorMessage);

				if(result != NULL) {
					LOG_MESSAGE(LogLevelError,
					            "Error while closing the websocket connection: read error: %s\n",
					            result);
				}

				FREE_AT_END();
				return NULL;
			}

			WebSocketRawMessage raw_message = raw_message_result.data.message;

			// some additional checks for control frames
			if(is_control_op_code(raw_message.opCode)) {

				if(!raw_message.fin) {
					CloseReason reason = { .code = CloseCode_ProtocolError,
						                   .message = "Received fragmented control frame",
						                   .message_len = -1 };

					const char* result =
					    close_websocket_connection(connection, argument->manager, reason);

					if(result != NULL) {
						LOG_MESSAGE(LogLevelError,
						            "Error while closing the websocket connection: "
						            "fragmented control frame: %s\n",
						            result);
					}

					FREE_RAW_WS_MESSAGE();
					FREE_AT_END();
					return NULL;
				}

				if(raw_message.payload_len > MAX_CONTROL_FRAME_PAYLOAD) {
					CloseReason reason = { .code = CloseCode_ProtocolError,
						                   .message = "Control frame payload to large",
						                   .message_len = -1 };

					const char* result =
					    close_websocket_connection(connection, argument->manager, reason);

					if(result != NULL) {
						LOG_MESSAGE(LogLevelError,
						            "Error while closing the websocket connection: "
						            "Control frame payload to large: %s\n",
						            result);
					}

					FREE_RAW_WS_MESSAGE();
					FREE_AT_END();
					return NULL;
				}

				// check utf-8 encoding in close messages
				if(raw_message.opCode == WS_OPCODE_CLOSE) {

					// the close message MAY contain additional data
					if(raw_message.payload_len != 0) {
						// the first two bytes are the code, so they have to be present, if size !=
						// 0 (so either 0 or >= 2)
						if(raw_message.payload_len < 2) {
							CloseReason reason = { .code = CloseCode_ProtocolError,
								                   .message = "Close data has invalid code, it has "
								                              "to be at least 2 bytes long",
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(connection, argument->manager, reason);

							if(result != NULL) {
								LOG_MESSAGE(LogLevelError,
								            "Error while closing the websocket connection: "
								            "Close data has invalid code: %s\n",
								            result);
							}

							FREE_RAW_WS_MESSAGE();
							FREE_AT_END();
							return NULL;
						}

						Utf8DataResult utf8_result =
						    get_utf8_string(((char*)(raw_message.payload)) + 2,
						                    (long)(raw_message.payload_len - 2));

						if(utf8_result.has_error) {
							char* errorMessage = NULL;
							formatString(
							    &errorMessage,
							    {
								    FREE_AT_END();
								    return NULL;
							    },
							    "Invalid utf8 payload in control frame: %s",
							    utf8_result.data.error);

							CloseReason reason = { .code = CloseCode_InvalidFramePayloadData,
								                   .message = errorMessage,
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(connection, argument->manager, reason);

							free(errorMessage);

							if(result != NULL) {
								LOG_MESSAGE(LogLevelError,
								            "Error while closing the websocket connection: "
								            "Invalid utf8 payload in control frame: %s\n",
								            result);
							}

							FREE_RAW_WS_MESSAGE();
							FREE_AT_END();
							return NULL;
						}

						Utf8Data data =
						    utf8_result.data.result; // NOLINT(clang-analyzer-unix.Malloc)
						// TODO(Totto): do something with this
						free(data.data);
					}
				}
			}

			switch(raw_message.opCode) {
				case WS_OPCODE_CONT: {
					if(!has_message) {
						CloseReason reason = {
							.code = CloseCode_ProtocolError,
							.message = "Received Opcode CONTINUATION, but no start frame received",
							.message_len = -1
						};

						const char* result =
						    close_websocket_connection(connection, argument->manager, reason);

						if(result != NULL) {
							LOG_MESSAGE(
							    LogLevelError,
							    "Error while closing the websocket connection: CONT error: %s\n",
							    result);
						}

						FREE_RAW_WS_MESSAGE();
						FREE_AT_END();
						return NULL;
					}

					uint64_t old_length = current_message.data_len;
					void* old_data = current_message.data;

					current_message.data = malloc(old_length + raw_message.payload_len);

					if(!current_message.data) {
						// TODO(Totto): report this error
						FREE_RAW_WS_MESSAGE();
						FREE_AT_END();
						return NULL;
					}

					current_message.data_len += raw_message.payload_len;

					if(old_length != 0 && old_data != NULL) {
						memcpy(current_message.data, old_data, old_length);
						free(old_data);
					}

					if(raw_message.payload_len != 0 && raw_message.payload != NULL) {
						memcpy(((uint8_t*)current_message.data) + old_length, raw_message.payload,
						       raw_message.payload_len);
						free(raw_message.payload);
					}

					if(!raw_message.fin) {
						continue;
					}

					if(current_message.is_text) {
						Utf8DataResult utf8_result =
						    get_utf8_string(current_message.data, (long)current_message.data_len);

						if(utf8_result.has_error) {

							char* errorMessage = NULL;
							// TODO(Totto): better report error
							formatString(&errorMessage, return NULL;
							             , "Invalid utf8 payload in fragmented message: %s",
							             utf8_result.data.error);

							CloseReason reason = { .code = CloseCode_InvalidFramePayloadData,
								                   .message = errorMessage,
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(connection, argument->manager, reason);

							free(errorMessage);

							if(result != NULL) {
								LOG_MESSAGE(LogLevelError,
								            "Error while closing the websocket connection: "
								            "Invalid utf8 payload in fragmented message: %s\n",
								            result);
							}

							FREE_RAW_WS_MESSAGE();
							FREE_AT_END();
							return NULL;
						}

						Utf8Data data = utf8_result.data.result;
						// TODO(Totto): do something with this
						free(data.data);
					}
					// can't break out of a switch and the while loop, so using goto
					goto handle_message;
				}

				case WS_OPCODE_TEXT:
				case WS_OPCODE_BIN: {
					if(has_message) {
						CloseReason reason = {
							.code = CloseCode_ProtocolError,
							.message =
							    "Received other opCode than CONTINUATION after the first fragment",
							.message_len = -1
						};

						const char* result =
						    close_websocket_connection(connection, argument->manager, reason);

						if(result != NULL) {
							LOG_MESSAGE(
							    LogLevelError,
							    "Error while closing the websocket connection: no CONT error: %s\n",
							    result);
						}

						FREE_RAW_WS_MESSAGE();
						FREE_AT_END();
						return NULL;
					}

					has_message = true;

					current_message.is_text =
					    raw_message.opCode == // NOLINT(readability-implicit-bool-conversion)
					    WS_OPCODE_TEXT;
					current_message.data = raw_message.payload;
					current_message.data_len = raw_message.payload_len;

					if(!raw_message.fin) {
						continue;
					}

					if(current_message.is_text) {
						Utf8DataResult utf8_result =
						    get_utf8_string(current_message.data, (long)current_message.data_len);

						if(utf8_result.has_error) {
							char* errorMessage = NULL;
							formatString(
							    &errorMessage,
							    {
								    FREE_RAW_WS_MESSAGE();
								    FREE_AT_END();
								    return NULL;
							    },
							    "Invalid utf8 payload in un-fragmented message: %s",
							    utf8_result.data.error);

							CloseReason reason = { .code = CloseCode_InvalidFramePayloadData,
								                   .message = errorMessage,
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(connection, argument->manager, reason);

							free(errorMessage);

							if(result != NULL) {
								LOG_MESSAGE(LogLevelError,
								            "Error while closing the websocket connection: "
								            "Invalid utf8 payload in un-fragmented message: %s\n",
								            result);
							}

							FREE_RAW_WS_MESSAGE();
							FREE_AT_END();
							return NULL;
						}

						Utf8Data data =
						    utf8_result.data.result; // NOLINT(clang-analyzer-unix.Malloc)
						// TODO(Totto): do something with this
						free(data.data);
					}

					// can't break out of a switch and the while loop, so using goto
					goto handle_message;
				}
				case WS_OPCODE_CLOSE: {

					CloseReason reason = { .code = CloseCode_NormalClosure,
						                   .message = "Planned close",
						                   .message_len = -1 };

					if(raw_message.payload_len != 0) {
						CloseReasonResult reason_parse_result =
						    maybe_parse_close_reason(raw_message, true);
						if(reason_parse_result.success) {

							CloseReason new_reason = reason_parse_result.reason;

							if(!is_valid_close_code(new_reason.code)) {
								CloseReason invalid_close_code_reason = {
									.code = CloseCode_ProtocolError,
									.message = "Invalid Close Code",
									.message_len = -1
								};

								const char* result = close_websocket_connection(
								    connection, argument->manager, invalid_close_code_reason);

								if(result != NULL) {
									LOG_MESSAGE(LogLevelError,
									            "Error while closing the websocket connection: "
									            "Invalid Close Code: %s\n",
									            result);
								}

								FREE_RAW_WS_MESSAGE();
								FREE_AT_END();
								return NULL;
							}

							reason = new_reason;
						}
					}

					const char* result =
					    close_websocket_connection(connection, argument->manager, reason);

					if(result != NULL) {
						LOG_MESSAGE(LogLevelError,
						            "Error while closing the websocket connection: planned "
						            "close: %s\n",
						            result);
					}

					FREE_RAW_WS_MESSAGE();
					FREE_AT_END();
					return NULL;
				}

				case WS_OPCODE_PING: {
					WebSocketRawMessage message_raw = { .fin = true,
						                                .opCode = WS_OPCODE_PONG,
						                                .payload = raw_message.payload,
						                                .payload_len = raw_message.payload_len };
					int result = ws_send_message_raw_internal(connection, message_raw, false);

					if(result < 0) {
						CloseReason reason = { .code = CloseCode_ProtocolError,
							                   .message = "Couldn't send PONG opCode",
							                   .message_len = -1 };

						const char* result1 =
						    close_websocket_connection(connection, argument->manager, reason);

						if(result1 != NULL) {
							LOG_MESSAGE(LogLevelError,
							            "Error while closing the websocket connection: PONG send "
							            "error: %s\n",
							            result1);
						}

						FREE_RAW_WS_MESSAGE();
						FREE_AT_END();
						return NULL;
					}

					continue;
				}

				case WS_OPCODE_PONG: {
					// just ignore
					continue;
				}

				default: {
					CloseReason reason = { .code = CloseCode_ProtocolError,
						                   .message = "Received Opcode that is not supported",
						                   .message_len = -1 };

					const char* result =
					    close_websocket_connection(connection, argument->manager, reason);

					if(result != NULL) {
						LOG_MESSAGE(LogLevelError,
						            "Error while closing the websocket connection: "
						            "Unsupported opCode: %s\n",
						            result);
					}

					FREE_RAW_WS_MESSAGE();
					FREE_AT_END();
					return NULL;
				}
			}
		}

	handle_message:

		if(has_message) {
			WebSocketAction action = connection->function(connection, current_message);
			free(current_message.data);

			if(action == WebSocketAction_Close) {
				CloseReason reason = { .code = CloseCode_NormalClosure,
					                   .message = "ServerApplication requested shutdown",
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(connection, argument->manager, reason);

				if(result != NULL) {
					LOG_MESSAGE(
					    LogLevelError,
					    "Error while closing the websocket connection: shutdown requested: %s\n",
					    result);
				}

				FREE_AT_END();
				return NULL;
			}

			if(action == WebSocketAction_Error) {
				CloseReason reason = { .code = CloseCode_ProtocolError,
					                   .message = "ServerApplication callback has an error",
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(connection, argument->manager, reason);

				if(result != NULL) {
					LOG_MESSAGE(LogLevelError,
					            "Error while closing the websocket connection: "
					            "callback has error: %s\n",
					            result);
				}

				FREE_AT_END();
				return NULL;
			}
		}
	}

	FREE_AT_END();
	return NULL;
}

#undef FREE_RAW_WS_MESSAGE

#undef FREE_AT_END

int ws_send_message(WebSocketConnection* connection, WebSocketMessage message) {

	return ws_send_message_internal(connection, message, false, WS_FRAGMENTATION_OFF);
}

int ws_send_message_fragmented(WebSocketConnection* connection, WebSocketMessage message,
                               int64_t fragment_size) {
	return ws_send_message_internal(connection, message, false, fragment_size);
}

WebSocketThreadManager* initialize_thread_manager(void) {

	WebSocketThreadManager* manager = malloc(sizeof(WebSocketThreadManager));

	if(!manager) {
		// TODO(Totto): better report error
		return NULL;
	}

	int result = pthread_mutex_init(&manager->mutex, NULL);
	// TODO(Totto): better report error
	checkForThreadError(
	    result,
	    "An Error occurred while trying to initialize the mutex for the WebSocketThreadManager",
	    return NULL;);
	manager->head = NULL;

	return manager;
}

WebSocketConnection* thread_manager_add_connection(WebSocketThreadManager* manager,
                                                   const ConnectionDescriptor* const descriptor,
                                                   ConnectionContext* context,
                                                   WebSocketFunction function) {

	int result = pthread_mutex_lock(&manager->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to lock the mutex for the WebSocketThreadManager",
	    return NULL;);

	WebSocketConnection* connection = malloc(sizeof(WebSocketConnection));

	if(!connection) {
		// TODO(Totto): better report error
		return NULL;
	}

	connection->context = context;
	connection->descriptor = descriptor;
	connection->function = function;

	struct connection_node_t* current_node = NULL;
	struct connection_node_t* next_node = manager->head;

	while(true) {
		if(next_node == NULL) {
			struct connection_node_t* new_node = malloc(sizeof(struct connection_node_t));

			if(!new_node) {
				// TODO(Totto): better report error
				free(connection);
				return NULL;
			}

			new_node->connection = connection;
			new_node->next = NULL;
			if(current_node == NULL) {
				manager->head = new_node;
			} else {
				current_node->next = new_node;
			}
			break;
		}

		current_node = next_node;
		next_node = current_node->next;
	}

	WebSocketListenerArg* threadArgument =
	    (WebSocketListenerArg*)malloc(sizeof(WebSocketListenerArg));

	if(!threadArgument) {
		// TODO(Totto): better report error
		return NULL;
	}

	// initializing the struct with the necessary values
	threadArgument->connection = connection;
	threadArgument->manager = manager;

	result = pthread_create(&connection->thread_id, NULL, ws_listener_function, threadArgument);
	// TODO(Totto): better report error
	checkForThreadError(result, "An Error occurred while trying to create a new Thread",
	                    return NULL;);

	result = pthread_mutex_unlock(&manager->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the WebSocketThreadManager",
	    return NULL;);

	return connection;
}

static void free_connection(WebSocketConnection* connection, bool send_go_away) {

	if(send_go_away) {
		CloseReason reason = { .code = CloseCode_GoingAway,
			                   .message = "Server is shutting down",
			                   .message_len = -1 };
		int result = ws_send_close_message_raw_internal(connection, reason);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error while closing the websocket connection: close "
			                                  "reason: server shutting down\n");
		}
	}

	close_connection_descriptor(connection->descriptor, connection->context);
	free_connection_context(connection->context);
	free(connection);
}

int thread_manager_remove_connection(WebSocketThreadManager* manager,
                                     WebSocketConnection* connection) {

	if(connection == NULL) {
		return -1;
	}

	int result = pthread_mutex_lock(&manager->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to lock the mutex for the WebSocketThreadManager",
	    return -2;);

	struct connection_node_t* current_node = manager->head;
	struct connection_node_t* previous_node = NULL;

	int return_value = -3;

	while(true) {
		if(current_node == NULL) {
			return_value = -4;
			break;
		}

		// TODO(Totto): shut down connection, if it is still running e.g. in the case of GET to
		// /shutdown

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
			return_value = 0;
			break;
		}

		previous_node = current_node;
		current_node = current_node->next;
	}

	result = pthread_mutex_unlock(&manager->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the WebSocketThreadManager",
	    return false;);

	return return_value;
}

bool thread_manager_remove_all_connections(WebSocketThreadManager* manager) {

	struct connection_node_t* current_node = manager->head;

	while(true) {
		if(current_node == NULL) {
			break;
		}

		// TODO(Totto): shut down connections, if they are still running e.g. in the case of GET to
		// /shutdown

		WebSocketConnection* connection = current_node->connection;

		int result = pthread_cancel(connection->thread_id);
		// TODO(Totto): better report error
		checkForError(result, "While trying to cancel a WebSocketConnection Thread", return false;);

		free_connection(connection, true);

		struct connection_node_t* to_free = current_node;

		current_node = current_node->next;
		free(to_free);
	}

	manager->head = NULL;
	return true;
}

bool free_thread_manager(WebSocketThreadManager* manager) {

	int result = pthread_mutex_destroy(&manager->mutex);
	// TODO(Totto): better report error
	checkForThreadError(result,
	                    "An Error occurred while trying to destroy the mutex in "
	                    "cleaning up for the WebSocketThreadManager",
	                    return false;);

	if(manager->head != NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "All connections got removed correctly");
		return false;
	}

	free(manager);
	return true;
}
