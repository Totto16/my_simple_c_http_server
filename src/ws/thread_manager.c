

#include "thread_manager.h"
#include "generic/hash.h"
#include "generic/helper.h"
#include "generic/read.h"
#include "generic/send.h"
#include "utils/log.h"
#include "utils/thread_helper.h"
#include "utils/thread_pool.h"
#include "utils/utf8_helper.h"
#include "utils/utils.h"

#include <arpa/inet.h>

#include <errno.h>
#include <pthread.h>

#include <stdlib.h>
#include <time.h>

#ifdef __APPLE__
#include <machine/endian.h>

#include "./macos_endian_compat.h"
#else
#include <endian.h>
#endif

typedef struct ConnectionNodeImpl ConnectionNode;

struct ConnectionNodeImpl {
	WebSocketConnection* connection;
	ConnectionNode* next;
};

struct WebSocketThreadManagerImpl {
	pthread_mutex_t mutex;
	ConnectionNode* head;
};

struct WebSocketConnectionImpl {
	ConnectionContext* context;
	ConnectionDescriptor* descriptor;
	WebSocketFunction function;
	pthread_t thread_id;
	WsConnectionArgs args;
	LifecycleFunctions fns;
};

typedef struct {
	bool fin;
	WsOpcode op_code;
	bool mask;
	uint8_t payload_len;
	uint8_t rsv_bytes;
} RawHeaderOne;

typedef struct {
	bool has_error;
	union {
		RawHeaderOne header;
		const char* error;
	} data;
} RawHeaderOneResult;

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

static void thread_manager_thread_startup_function(void) {
#ifdef _SIMPLE_SERVER_USE_OPENSSL
	openssl_initialize_crypto_thread_state();
#endif

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Running startup function for ws thread\n");
}

static void thread_manager_thread_shutdown_function(void) {
#ifdef _SIMPLE_SERVER_USE_OPENSSL
	openssl_cleanup_crypto_thread_state();
#endif

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Running shutdown function for ws thread\n");
}

#define WS_ALLOW_SSL_CONTEXT_REUSE false

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
get_raw_header(uint8_t const header_bytes[RAW_MESSAGE_HEADER_SIZE], uint8_t allowed_rsv_bytes) {
	bool fin = (header_bytes[0] >> // NOLINT(readability-implicit-bool-conversion)
	            7 // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	            ) &
	           0b1; // NOLINT(readability-implicit-bool-conversion)
	uint8_t rsv_bytes =
	    (header_bytes[0] >> 4) &
	    0b111; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	if(allowed_rsv_bytes == 0) {
		if(rsv_bytes != 0) {
			return (RawHeaderOneResult){ .has_error = true,
				                         .data = { .error = "only 0 allowed for the rsv bytes" } };
		};
	} else {

		if((rsv_bytes | allowed_rsv_bytes) != allowed_rsv_bytes) {
			return (RawHeaderOneResult){ .has_error = true,
				                         .data = { .error = "invalid rsv bits set" } };
		}
	}

	WsOpcode op_code =
	    header_bytes[0] &
	    0b1111; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	bool mask = (header_bytes[1] >> // NOLINT(readability-implicit-bool-conversion)
	             7) & // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	            0b1;  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t payload_len =
	    header_bytes[1] &
	    0x7F; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	RawHeaderOne header = { .fin = fin,
		                    .op_code = op_code,
		                    .mask = mask,
		                    .payload_len = payload_len,
		                    .rsv_bytes = rsv_bytes };

	RawHeaderOneResult result = { .has_error = false, .data = { .header = header } };
	return result;
}

NODISCARD static bool is_control_op_code(WsOpcode op_code) {
	// wrong opcodes, they only can be 4 bytes large
	if(op_code > 0xF) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		return false;
	}

	return (op_code &  // NOLINT(readability-implicit-bool-conversion)
	        0b1000) != // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	       0;
}

NODISCARD static WebSocketRawMessageResult
read_raw_message(WebSocketConnection* connection,
                 ExtensionReceivePipelineSettings pipeline_settings) {

	uint8_t* header_bytes =
	    (uint8_t*)read_exact_bytes(connection->descriptor, RAW_MESSAGE_HEADER_SIZE);
	if(!header_bytes) {
		return (WebSocketRawMessageResult){ .has_error = true,
			                                .data = { .error = "couldn't read header bytes (2)" } };
	}

	RawHeaderOneResult raw_header_result =
	    get_raw_header(header_bytes, pipeline_settings.allowed_rsv_bytes);

	free(header_bytes);

	if(raw_header_result.has_error) {
		return (WebSocketRawMessageResult){ .has_error = true,
			                                .data = { .error = raw_header_result.data.error } };
	}

	RawHeaderOne raw_header = raw_header_result.data.header;

	uint64_t payload_len = (uint64_t)raw_header.payload_len;

	if(payload_len == EXTENDED_PAYLOAD_MAGIC_NUMBER1) {
		uint16_t* payload_len_result =
		    (uint16_t*)read_exact_bytes(connection->descriptor, RAW_MESSAGE_PAYLOAD_1_SIZE);
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
		    (uint64_t*)read_exact_bytes(connection->descriptor, RAW_MESSAGE_PAYLOAD_2_SIZE);
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
		mask_byte = (uint8_t*)read_exact_bytes(connection->descriptor, RAW_MESSAGE_MASK_BYTE_SIZE);
		if(!mask_byte) {
			return (WebSocketRawMessageResult){
				.has_error = true, .data = { .error = "couldn't read mask bytes (4)" }
			};
		}
	}

	void* payload = NULL;

	if(payload_len != 0) {

		payload = read_exact_bytes(connection->descriptor, payload_len);
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
		                          .op_code = raw_header.op_code,
		                          .payload = payload,
		                          .payload_len = payload_len,
		                          .rsv_bytes = raw_header.rsv_bytes };

	WebSocketRawMessageResult result = { .has_error = false, .data = { .message = value } };

	return result;
}

NODISCARD static int ws_send_message_raw_internal(WebSocketConnection* connection,
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

	uint8_t* resulting_frame = (uint8_t*)malloc(size);

	if(resulting_frame == NULL) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return -1;
	}

	uint8_t header_one =
	    ((raw_message.fin & 0b1) // NOLINT(readability-implicit-bool-conversion)
	     << 7) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	    | ((raw_message.rsv_bytes & 0b111) << 4) |
	    (raw_message.op_code &
	     0b1111); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	uint8_t additional_payload_len_2 = payload_additional_len == 2 ? EXTENDED_PAYLOAD_MAGIC_NUMBER1
	                                                               : EXTENDED_PAYLOAD_MAGIC_NUMBER2;

	uint8_t payload_len_1 =
	    payload_additional_len == 0 ? raw_message.payload_len : additional_payload_len_2;

	uint8_t header_two =
	    ((mask & 0b1) // NOLINT(readability-implicit-bool-conversion)
	     << 7) |      // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	    (payload_len_1 &
	     0x7F); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	resulting_frame[0] = header_one;
	resulting_frame[1] = header_two;

	if(payload_additional_len == 2) {
		// in network byte order
		*((uint16_t*)(resulting_frame + 2)) = htons((uint16_t)(raw_message.payload_len));
	} else if(payload_additional_len ==
	          8) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		// in network byte order (alias big endian = be)
		*((uint64_t*)(resulting_frame + 2)) = htobe64(raw_message.payload_len);
	}

	if(raw_message.payload_len != 0) {
		memcpy(resulting_frame + header_offset, raw_message.payload, raw_message.payload_len);
	}

	if(mask) {

		uint32_t mask_byte = get_random_byte();

		*((uint32_t*)(resulting_frame + RAW_MESSAGE_HEADER_SIZE + payload_additional_len)) =
		    mask_byte;

		for(size_t i = 0; i < raw_message.payload_len; ++i) {
			(resulting_frame + header_offset)[i] =
			    ((uint8_t*)raw_message.payload)[i] ^ ((uint8_t*)(&mask_byte))[i % 4];
		}
	}

	if(is_control_op_code(raw_message.op_code)) {
		if(!raw_message.fin) {
			// TODO(Totto): add error message
			LOG_MESSAGE_SIMPLE(LogLevelDebug,
			                   "Control frame payload is fragmented, that isn't allowed\n");

			free(resulting_frame);
			return -2;
		}

		if(raw_message.payload_len > MAX_CONTROL_FRAME_PAYLOAD) {
			// TODO(Totto): add error message
			LOG_MESSAGE(LogLevelDebug,
			            "Control frame payload length is too large: %" PRIu64 " > %d\n",
			            raw_message.payload_len, MAX_CONTROL_FRAME_PAYLOAD);

			free(resulting_frame);
			return -3;
		}
	}

	int result = send_data_to_connection(connection->descriptor, resulting_frame, size);

	free(resulting_frame);

	return result;
}

NODISCARD static int ws_send_message_internal_normal(WebSocketConnection* connection,
                                                     WebSocketMessage* message, bool mask,
                                                     ExtensionSendState* extension_send_state) {

	WsOpcode op_code = message->is_text // NOLINT(readability-implicit-bool-conversion)
	                       ? WsOpcodeText
	                       : WsOpcodeBin;

	WebSocketRawMessage raw_message = { .fin = true,
		                                .op_code = op_code,
		                                .payload = message->data,
		                                .payload_len = message->data_len,
		                                .rsv_bytes = 0b000 };

	extension_send_pipeline_process_start_message(extension_send_state, &raw_message);

	return ws_send_message_raw_internal(connection, raw_message, mask);
}

// according to rfc
#define WS_MINIMUM_FRAGMENT_SIZE 16

NODISCARD static int ws_send_message_internal_fragmented(WebSocketConnection* connection,
                                                         WebSocketMessage* message, bool mask,
                                                         uint64_t fragment_size,
                                                         ExtensionSendState* extension_send_state) {

	// this is the minimum we set, so that everything (header + eventual mask) can be sent
	if(fragment_size < WS_MINIMUM_FRAGMENT_SIZE) {
		fragment_size = WS_MINIMUM_FRAGMENT_SIZE;
	}

	if(message->data_len < fragment_size) {
		return ws_send_message_internal_normal(connection, message, mask, extension_send_state);
	}

	for(uint64_t start = 0; start < message->data_len; start += fragment_size) {
		uint64_t end = start + fragment_size;
		bool fin = false;
		uint64_t payload_len = fragment_size;

		if(end >= message->data_len) {
			end = message->data_len;
			fin = true;
			payload_len = end - start;
		}

		WsOpcode op_code =
		    start == 0
		        ? (message->is_text // NOLINT(readability-implicit-bool-conversion,readability-avoid-nested-conditional-operator)
		               ? WsOpcodeText
		               : WsOpcodeBin)
		        : WsOpcodeCont;

		void* payload = ((uint8_t*)message->data) + start;

		WebSocketRawMessage raw_message = { .fin = fin,
			                                .op_code = op_code,
			                                .payload = payload,
			                                .payload_len = payload_len,
			                                .rsv_bytes = 0b000 };
		if(start) {
			extension_send_pipeline_process_start_message(extension_send_state, &raw_message);
		} else {
			extension_send_pipeline_process_cont_message(extension_send_state, &raw_message);
		}

		int result = ws_send_message_raw_internal(connection, raw_message, mask);

		if(result < 0) {
			return result;
		}
	}

	return 0;
}

#define DEFAULT_AUTO_FRAGMENT_SIZE 4096

NODISCARD static int ws_send_message_internal(WebSocketConnection* connection,
                                              WebSocketMessage* message, bool mask,
                                              WsConnectionArgs args,
                                              ExtensionSendState* extension_send_state) {

	char* extension_error =
	    extension_send_pipeline_process_initial_message(extension_send_state, message);

	if(extension_error != NULL) {
		LOG_MESSAGE(LogLevelError, "Extension send error: %s\n", extension_error);
		free(extension_error);
		return -1;
	}

	if(args.fragment_option.type == WsFragmentOptionTypeOff) {
		return ws_send_message_internal_normal(connection, message, mask, extension_send_state);
	}

	if(args.fragment_option.type == WsFragmentOptionTypeAuto) {

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
					// NOTE: this value is the doubled, if you set it, but we use the doubled value
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

		return ws_send_message_internal_fragmented(connection, message, mask, chosen_fragment_size,
		                                           extension_send_state);
	}

	if(args.fragment_option.type == WsFragmentOptionTypeSet) {
		return ws_send_message_internal_fragmented(connection, message, mask,
		                                           args.fragment_option.data.set.fragment_size,
		                                           extension_send_state);
	}

	return ws_send_message_internal_normal(connection, message, mask, extension_send_state);
}

typedef C_23_ENUM_TYPE(uint16_t) CloseCodeEnumType;

#define CLOSE_CODE_CUSTOM(c) (CloseCode)((CloseCodeEnumType)(c))

#define CLOSE_CODE_ZERO CLOSE_CODE_CUSTOM(0)

/**
 * @enum value
 * @see https://datatracker.ietf.org/doc/html/rfc6455#section-11.7
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	CloseCodeNormalClosure = 1000,
	CloseCodeGoingAway = 1001,
	CloseCodeProtocolError = 1002,
	CloseCodeUnsupportedData = 1003,
	//
	CloseCodeInvalidFramePayloadData = 1007,
	CloseCodePolicyViolation = 1008,
	CloseCodeMessageTooBig = 1009,
	CloseCodeMandatoryExtension = 1010,
	CloseCodeInternalServerError = 1011
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

NODISCARD static CloseReasonResult maybe_parse_close_reason(WebSocketRawMessage raw_message,
                                                            bool also_parse_message) {
	if(raw_message.op_code != WsOpcodeClose) {
		return (CloseReasonResult){
			.success = false,
			.reason = { .code =
			                CLOSE_CODE_ZERO, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			            .message = NULL,
			            .message_len = 0 }
		};
	}

	uint64_t payload_len = raw_message.payload_len;

	if(payload_len < 2) {
		return (CloseReasonResult){
			.success = false,
			.reason = { .code =
			                CLOSE_CODE_ZERO, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
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
			.reason = { .code =
			                CLOSE_CODE_ZERO, // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
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
NODISCARD static bool is_valid_close_code(uint16_t close_code) {
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

NODISCARD static int ws_send_close_message_raw_internal(WebSocketConnection* connection,
                                                        CloseReason reason) {

	size_t reason_msg_len =
	    (reason.message_len < 0 ? strlen(reason.message) : (size_t)reason.message_len);

	size_t message_len = reason.message ? reason_msg_len : 0;

	uint64_t payload_len = 2 + message_len;

	uint8_t* payload = (uint8_t*)malloc(payload_len);

	if(payload == NULL) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return -1;
	}

	uint8_t* reason_code = (uint8_t*)(&reason.code);

	// network byte order
	payload[0] = reason_code[1];
	payload[1] = reason_code[0];

	if(reason.message) {
		memcpy(payload + 2, reason.message, message_len);
	}

	WebSocketRawMessage raw_message = { .fin = true,
		                                .op_code = WsOpcodeClose,
		                                .payload = payload,
		                                .payload_len = payload_len,
		                                .rsv_bytes = 0b000 };

	// TODO(Totto): once we support extensions, that needs to be run on control message, we need to
	// add that pipline step also here

	int result = ws_send_message_raw_internal(connection, raw_message, false);

	free(payload);

	return result;
}

NODISCARD static const char* close_websocket_connection(WebSocketConnection** connection,
                                                        WebSocketThreadManager* manager,
                                                        CloseReason reason) {

	if(reason.message != NULL) {
		int message_size =
		    reason.message_len < 0 ? (int)strlen(reason.message) : reason.message_len;
		LOG_MESSAGE(LogLevelTrace, "Closing the websocket connection: %.*s\n", message_size,
		            reason.message);
	} else {
		LOG_MESSAGE_SIMPLE(LogLevelTrace, "Closing the websocket connection: (no message)\n");
	}

	int result = ws_send_close_message_raw_internal(*connection, reason);

	RUN_LIFECYCLE_FN((*connection)->fns.shutdown_fn);

	// even if above failed, we need to remove the connection nevertheless
	int result2 = thread_manager_remove_connection(manager, *connection);

	*connection = NULL;

	if(result < 0) {
		return "send error";
	}

	if(result2 < 0) {
		return "thread manager remove error";
	}

	return NULL;
}

// TODO(Totto): at the moment we adhere to the RFC, by only checking TEXT as a whole after we got
// all fragments, but the autobahn test suggest, that we may fail fast, if the reason for an utf-8
// error isn't missing bytes at the end of the payload, like e.g. if we receive a whole invalid
// sequence
// note: regarding utf8 parsing

static ANY_TYPE(NULL) ws_listener_function(ANY_TYPE(WebSocketListenerArg*) arg_ign) {

	WebSocketListenerArg* argument = (WebSocketListenerArg*)arg_ign;

	char* thread_name_buffer = NULL;
	// TODO(Totto): better report error
	FORMAT_STRING(&thread_name_buffer, return NULL;, "ws listener " PRI_THREADID, get_thread_id());
	set_thread_name(thread_name_buffer);

#define FREE_ADDITIONALLY() \
	do { \
	} while(false)

#define FREE_AT_END_ONE() \
	do { \
		unset_thread_name(); \
		free(thread_name_buffer); \
		free(argument); \
		FREE_ADDITIONALLY(); \
	} while(false)

	bool sigpipe_result = setup_sigpipe_signal_handler();

	if(!sigpipe_result) {
		FREE_AT_END_ONE();
		return NULL;
	}

#define FREE_AT_END() \
	do { \
		FREE_AT_END_ONE(); \
		if(connection != NULL) { \
			RUN_LIFECYCLE_FN(connection->fns.shutdown_fn); \
		} \
	} while(false)

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting WS Listener\n");

	WebSocketConnection* connection = argument->connection;

	RUN_LIFECYCLE_FN(connection->fns.startup_fn);

	WSExtensions extensions = connection->args.extensions;

	ExtensionPipeline* extension_pipeline = get_extension_pipeline(extensions);

	if(extension_pipeline == NULL) {
		FREE_AT_END();
		return NULL;
	}

#define FREE_CURRENT_STATE() \
	do { \
	} while(false)

#undef FREE_ADDITIONALLY
#define FREE_ADDITIONALLY() \
	do { \
		free_extension_pipeline(extension_pipeline); \
		FREE_CURRENT_STATE(); \
	} while(false)

	ExtensionReceivePipelineSettings pipeline_receive_settings =
	    get_extension_receive_pipeline_settings(extension_pipeline);

	while(true) {

#undef FREE_CURRENT_STATE
#define FREE_CURRENT_STATE() \
	do { \
		free_extension_receive_message_state(message_receive_state); \
	} while(false)

		bool has_message = false;
		WebSocketMessage current_message = { .is_text = true, .data = NULL, .data_len = 0 };
		ExtensionMessageReceiveState* message_receive_state =
		    init_extension_receive_message_state(extension_pipeline);

		while(true) {

			WebSocketRawMessageResult raw_message_result =
			    read_raw_message(connection, pipeline_receive_settings);

#define FREE_RAW_WS_MESSAGE() \
	do { \
		if(has_message) /*NOLINT(bugprone-redundant-branch-condition)*/ { \
			free_ws_message(current_message); \
		} else { \
			free_raw_ws_message(raw_message); \
		} \
	} while(false)

			if(raw_message_result.has_error) {

				char* error_message = NULL;
				FORMAT_STRING(
				    &error_message,
				    {
					    FREE_AT_END();
					    return NULL;
				    },
				    "Error while reading the needed bytes for a frame: %s",
				    raw_message_result.data.error);

				LOG_MESSAGE(LogLevelInfo, "%s\n", error_message);

				CloseReason reason = { .code = CloseCodeProtocolError,
					                   .message = error_message,
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(&connection, argument->manager, reason);

				free(error_message);

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
			if(is_control_op_code(raw_message.op_code)) {

				if(!raw_message.fin) {
					CloseReason reason = { .code = CloseCodeProtocolError,
						                   .message = "Received fragmented control frame",
						                   .message_len = -1 };

					const char* result =
					    close_websocket_connection(&connection, argument->manager, reason);

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
					CloseReason reason = { .code = CloseCodeProtocolError,
						                   .message = "Control frame payload to large",
						                   .message_len = -1 };

					const char* result =
					    close_websocket_connection(&connection, argument->manager, reason);

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
				if(raw_message.op_code == WsOpcodeClose) {

					// the close message MAY contain additional data
					if(raw_message.payload_len != 0) {
						// the first two bytes are the code, so they have to be present, if size !=
						// 0 (so either 0 or >= 2)
						if(raw_message.payload_len < 2) {
							CloseReason reason = { .code = CloseCodeProtocolError,
								                   .message = "Close data has invalid code, it has "
								                              "to be at least 2 bytes long",
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(&connection, argument->manager, reason);

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
							char* error_message = NULL;
							FORMAT_STRING(
							    &error_message,
							    {
								    FREE_AT_END();
								    return NULL;
							    },
							    "Invalid utf8 payload in control frame: %s",
							    utf8_result.data.error);

							CloseReason reason = { .code = CloseCodeInvalidFramePayloadData,
								                   .message = error_message,
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(&connection, argument->manager, reason);

							free(error_message);

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
						free_utf8_data(data);
					}
				}
			}

			switch(raw_message.op_code) {
				case WsOpcodeCont: {
					if(!has_message) {
						CloseReason reason = {
							.code = CloseCodeProtocolError,
							.message = "Received Opcode CONTINUATION, but no start frame received",
							.message_len = -1
						};

						const char* result =
						    close_websocket_connection(&connection, argument->manager, reason);

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

					char* extension_error = extension_receive_pipeline_is_valid_cont_frame(
					    extension_pipeline, message_receive_state, raw_message);

					if(extension_error != NULL) {
						CloseReason reason = { .code = CloseCodeProtocolError,
							                   .message = extension_error,
							                   .message_len = -1 };

						const char* result =
						    close_websocket_connection(&connection, argument->manager, reason);

						free(extension_error);

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

					char* finish_error = extension_receive_pipeline_process_finished_message(
					    extension_pipeline, message_receive_state, &current_message);

					if(finish_error != NULL) {
						char* error_message = NULL;
						FORMAT_STRING(&error_message, FREE_AT_END(); return NULL;
						              , "Couldn't parse message at end: %s", finish_error);

						free(finish_error);

						CloseReason reason = { .code = CloseCodeInvalidFramePayloadData,
							                   .message = error_message,
							                   .message_len = -1 };

						const char* result =
						    close_websocket_connection(&connection, argument->manager, reason);

						free(error_message);

						if(result != NULL) {
							LOG_MESSAGE(LogLevelError,
							            "Error while closing the websocket connection: "
							            "Extension pipeline error: %s\n",
							            result);
						}

						FREE_RAW_WS_MESSAGE();
						FREE_AT_END();
						return NULL;
					}

					if(current_message.is_text) {
						Utf8DataResult utf8_result =
						    get_utf8_string(current_message.data, (long)current_message.data_len);

						if(utf8_result.has_error) {

							char* error_message = NULL;
							// TODO(Totto): better report error
							FORMAT_STRING(&error_message, FREE_AT_END(); return NULL;
							              , "Invalid utf8 payload in fragmented message: %s",
							              utf8_result.data.error);

							CloseReason reason = { .code = CloseCodeInvalidFramePayloadData,
								                   .message = error_message,
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(&connection, argument->manager, reason);

							free(error_message);

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
						free_utf8_data(data);
					}
					// can't break out of a switch and the while loop, so using goto
					goto handle_message;
				}

				case WsOpcodeText:
				case WsOpcodeBin: {
					if(has_message) {
						CloseReason reason = {
							.code = CloseCodeProtocolError,
							.message =
							    "Received other op_code than CONTINUATION after the first fragment",
							.message_len = -1
						};

						const char* result =
						    close_websocket_connection(&connection, argument->manager, reason);

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
					extension_receive_pipeline_process_start_message(
					    extension_pipeline, message_receive_state, raw_message);

					current_message.is_text =
					    raw_message.op_code == // NOLINT(readability-implicit-bool-conversion)
					    WsOpcodeText;
					current_message.data = raw_message.payload;
					current_message.data_len = raw_message.payload_len;

					if(!raw_message.fin) {
						continue;
					}

					char* finish_error = extension_receive_pipeline_process_finished_message(
					    extension_pipeline, message_receive_state, &current_message);

					if(finish_error != NULL) {
						char* error_message = NULL;
						FORMAT_STRING(&error_message, FREE_AT_END(); return NULL;
						              , "Couldn't parse message at end: %s", finish_error);

						free(finish_error);

						CloseReason reason = { .code = CloseCodeInvalidFramePayloadData,
							                   .message = error_message,
							                   .message_len = -1 };

						const char* result =
						    close_websocket_connection(&connection, argument->manager, reason);

						free(error_message);

						if(result != NULL) {
							LOG_MESSAGE(LogLevelError,
							            "Error while closing the websocket connection: "
							            "Extension pipeline error: %s\n",
							            result);
						}

						FREE_RAW_WS_MESSAGE();
						FREE_AT_END();
						return NULL;
					}

					if(current_message.is_text) {
						Utf8DataResult utf8_result =
						    get_utf8_string(current_message.data, (long)current_message.data_len);

						if(utf8_result.has_error) {
							char* error_message = NULL;
							FORMAT_STRING(
							    &error_message,
							    {
								    FREE_RAW_WS_MESSAGE();
								    FREE_AT_END();
								    return NULL;
							    },
							    "Invalid utf8 payload in un-fragmented message: %s",
							    utf8_result.data.error);

							CloseReason reason = { .code = CloseCodeInvalidFramePayloadData,
								                   .message = error_message,
								                   .message_len = -1 };

							const char* result =
							    close_websocket_connection(&connection, argument->manager, reason);

							free(error_message);

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
						free_utf8_data(data);
					}

					// can't break out of a switch and the while loop, so using goto
					goto handle_message;
				}
				case WsOpcodeClose: {

					CloseReason reason = { .code = CloseCodeNormalClosure,
						                   .message = "Planned close",
						                   .message_len = -1 };

					if(raw_message.payload_len != 0) {
						CloseReasonResult reason_parse_result =
						    maybe_parse_close_reason(raw_message, true);
						if(reason_parse_result.success) {

							CloseReason new_reason = reason_parse_result.reason;

							if(!is_valid_close_code(new_reason.code)) {
								CloseReason invalid_close_code_reason = {
									.code = CloseCodeProtocolError,
									.message = "Invalid Close Code",
									.message_len = -1
								};

								const char* result = close_websocket_connection(
								    &connection, argument->manager, invalid_close_code_reason);

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
					    close_websocket_connection(&connection, argument->manager, reason);

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

				case WsOpcodePing: {
					WebSocketRawMessage message_raw = { .fin = true,
						                                .op_code = WsOpcodePong,
						                                .payload = raw_message.payload,
						                                .payload_len = raw_message.payload_len,
						                                .rsv_bytes = raw_message.rsv_bytes };

					// TODO(Totto): once we support extensions, that needs to be run on control
					// message, we need to
					// add that pipline step also here

					int result = ws_send_message_raw_internal(connection, message_raw, false);

					if(result < 0) {
						CloseReason reason = { .code = CloseCodeProtocolError,
							                   .message = "Couldn't send PONG op_code",
							                   .message_len = -1 };

						const char* result1 =
						    close_websocket_connection(&connection, argument->manager, reason);

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

				case WsOpcodePong: {
					// just ignore
					continue;
				}

				default: {
					CloseReason reason = { .code = CloseCodeProtocolError,
						                   .message = "Received Opcode that is not supported",
						                   .message_len = -1 };

					const char* result =
					    close_websocket_connection(&connection, argument->manager, reason);

					if(result != NULL) {
						LOG_MESSAGE(LogLevelError,
						            "Error while closing the websocket connection: "
						            "Unsupported op_code: %s\n",
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

			ExtensionSendState* extension_send_state =
			    pipline_get_extension_send_state(extension_pipeline, message_receive_state);

			if(!extension_send_state) {
				CloseReason reason = { .code = CloseCodeProtocolError,
					                   .message = "Coudln't form the send extension state",
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(&connection, argument->manager, reason);

				if(result != NULL) {
					LOG_MESSAGE_SIMPLE(LogLevelError,
					                   "Error while closing the websocket connection: "
					                   "send extension state allocation error\n");
				}

				FREE_AT_END();
				return NULL;
			}

			WebSocketAction action = connection->function(connection, &current_message,
			                                              connection->args, extension_send_state);

			free_ws_message(current_message);

			free_extension_send_state(extension_send_state);

			// has_message = false;
			current_message.data = NULL;
			current_message.data_len = 0;

			if(action == WebSocketActionClose) {
				CloseReason reason = { .code = CloseCodeNormalClosure,
					                   .message = "ServerApplication requested shutdown",
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(&connection, argument->manager, reason);

				if(result != NULL) {
					LOG_MESSAGE(
					    LogLevelError,
					    "Error while closing the websocket connection: shutdown requested: %s\n",
					    result);
				}

				FREE_AT_END();
				return NULL;
			}

			if(action == WebSocketActionError) {
				CloseReason reason = { .code = CloseCodeProtocolError,
					                   .message = "ServerApplication callback has an error",
					                   .message_len = -1 };

				const char* result =
				    close_websocket_connection(&connection, argument->manager, reason);

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

		free_extension_receive_message_state(message_receive_state);

#undef FREE_ADDITIONALLY
#undef FREE_CURRENT_STATE
#define FREE_ADDITIONALLY() \
	do { \
	} while(false)
	}

	FREE_AT_END();
	return NULL;
}

#undef FREE_ADDITIONALLY
#undef FREE_RAW_WS_MESSAGE

#undef FREE_AT_END

int ws_send_message(WebSocketConnection* connection, WebSocketMessage* message,
                    WsConnectionArgs args, ExtensionSendState* extension_send_state) {
	return ws_send_message_internal(connection, message, false, args, extension_send_state);
}

void free_ws_message(WebSocketMessage message) {
	free(message.data);
}

void free_raw_ws_message(WebSocketRawMessage message) {
	free(message.payload);
}

WebSocketThreadManager* initialize_thread_manager(void) {

	WebSocketThreadManager* manager = malloc(sizeof(WebSocketThreadManager));

	if(!manager) {
		// TODO(Totto): better report error
		return NULL;
	}

	int result = pthread_mutex_init(&manager->mutex, NULL);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result,
	    "An Error occurred while trying to initialize the mutex for the WebSocketThreadManager",
	    return NULL;);
	manager->head = NULL;

	return manager;
}

WebSocketConnection* thread_manager_add_connection(WebSocketThreadManager* manager,
                                                   ConnectionDescriptor* const descriptor,
                                                   ConnectionContext* context,
                                                   WebSocketFunction function,
                                                   WsConnectionArgs args) {

	int result = pthread_mutex_lock(&manager->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
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
	connection->args = args;
	connection->fns =
	    (LifecycleFunctions){ .startup_fn = thread_manager_thread_startup_function,
		                      .shutdown_fn = thread_manager_thread_shutdown_function };

	ConnectionNode* current_node = NULL;
	ConnectionNode* next_node = manager->head;

	while(true) {
		if(next_node == NULL) {
			ConnectionNode* new_node = malloc(sizeof(ConnectionNode));

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

	WebSocketListenerArg* thread_argument =
	    (WebSocketListenerArg*)malloc(sizeof(WebSocketListenerArg));

	if(!thread_argument) {
		// TODO(Totto): better report error
		return NULL;
	}

	// initializing the struct with the necessary values
	thread_argument->connection = connection;
	thread_argument->manager = manager;

	result = pthread_create(&connection->thread_id, NULL, ws_listener_function, thread_argument);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(result, "An Error occurred while trying to create a new Thread",
	                       return NULL;);

	result = pthread_detach(connection->thread_id);
	CHECK_FOR_THREAD_ERROR(result,
	                       "An Error occurred while trying to detach the new WS connection Thread",
	                       return NULL;);

	result = pthread_mutex_unlock(&manager->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the WebSocketThreadManager",
	    return NULL;);

	return connection;
}

static void free_connection_args(WsConnectionArgs args) {
	stbds_arrfree(args.extensions);
}

static void free_connection(WebSocketConnection* connection, bool send_go_away) {

	if(send_go_away) {
		CloseReason reason = { .code = CloseCodeGoingAway,
			                   .message = "Server is shutting down",
			                   .message_len = -1 };
		int result = ws_send_close_message_raw_internal(connection, reason);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error while closing the websocket connection: close "
			                                  "reason: server shutting down\n");
		}
	}

	close_connection_descriptor_advanced(connection->descriptor, connection->context,
	                                     WS_ALLOW_SSL_CONTEXT_REUSE);
	free_connection_context(connection->context);
	free_connection_args(connection->args);
	free(connection);
}

int thread_manager_remove_connection(WebSocketThreadManager* manager,
                                     WebSocketConnection* connection) {

	if(connection == NULL) {
		return -1;
	}

	int result = pthread_mutex_lock(&manager->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to lock the mutex for the WebSocketThreadManager",
	    return -2;);

	ConnectionNode* current_node = manager->head;
	ConnectionNode* previous_node = NULL;

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

			ConnectionNode* next_node = current_node->next;

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
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the WebSocketThreadManager",
	    return false;);

	return return_value;
}

bool thread_manager_remove_all_connections(WebSocketThreadManager* manager) {

	ConnectionNode* current_node = manager->head;

	while(true) {
		if(current_node == NULL) {
			break;
		}

		// TODO(Totto): shut down connections, if they are still running e.g. in the case of GET
		// to /shutdown

		WebSocketConnection* connection = current_node->connection;

		int result = pthread_cancel(connection->thread_id);
		// TODO(Totto): better report error
		CHECK_FOR_ERROR(result, "While trying to cancel a WebSocketConnection Thread",
		                return false;);

		free_connection(connection, true);

		ConnectionNode* to_free = current_node;

		current_node = current_node->next;
		free(to_free);
	}

	manager->head = NULL;
	return true;
}

bool free_thread_manager(WebSocketThreadManager* manager) {

	int result = pthread_mutex_destroy(&manager->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(result,
	                       "An Error occurred while trying to destroy the mutex in "
	                       "cleaning up for the WebSocketThreadManager",
	                       return false;);

	if(manager->head != NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "All connections got removed correctly\n");
		return false;
	}

	free(manager);
	return true;
}
