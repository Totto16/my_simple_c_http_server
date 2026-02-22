
#include "read.h"

#include "utils/log.h"

#define MESSAGE_BUF_SIZE 1024

typedef struct {
	size_t capacity;
	SizedBuffer result;
} SizedBufferWithCap;

NODISCARD static SizedBufferWithCap
read_buffer_from_connection_impl(const ConnectionDescriptor* const descriptor) {
	// this buffer expands using realloc!!
	// also not the + 1 and the zero initialization, means that it's null terminated
	uint8_t* message_buffer = (uint8_t*)malloc(MESSAGE_BUF_SIZE);

	if(!message_buffer) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return (SizedBufferWithCap){ .capacity = 0 };
	}

	size_t buffers_used = 0;
	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarios
		ReadResult read_result = read_from_descriptor(
		    descriptor, message_buffer + (MESSAGE_BUF_SIZE * buffers_used), MESSAGE_BUF_SIZE);

		if(read_result.type == ReadResultTypeError) {
			// NOTE. make a function that gets the error from openssl!
			LOG_MESSAGE(LogLevelWarn, "Couldn't read from a connection: %s\n",
			            get_read_error_meaning(descriptor, read_result.data.opaque_error));

			free(message_buffer);
			return (SizedBufferWithCap){ .capacity = 0 };
		}

		if(read_result.type == ReadResultTypeEOF) {
			size_t total_size = (MESSAGE_BUF_SIZE * buffers_used);

			// client disconnected, so done
			LOG_MESSAGE_SIMPLE(LogLevelTrace, "client disconnected\n");
			return (SizedBufferWithCap){ .capacity = total_size,
				                         .result = (SizedBuffer){ .data = message_buffer,
				                                                  .size = total_size } };
		}

		assert(read_result.type == ReadResultTypeSuccess);

		size_t bytes_read = read_result.data.bytes_read;

		if(bytes_read == MESSAGE_BUF_SIZE) {
			// now the buffer has to be reused, so it's re-allocated, the used realloc helper also
			// initializes it with 0 and copies the old content, so nothing is lost and a new
			// MESSAGE_BUF_SIZE capacity is available + a null byte at the end
			size_t old_size = ((buffers_used + 1) * MESSAGE_BUF_SIZE);
			uint8_t* new_buffer =
			    (uint8_t*)realloc(message_buffer, (old_size + MESSAGE_BUF_SIZE + 1));

			if(!new_buffer) {
				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
				                   "Couldn't re-allocate memory!\n");

				free(message_buffer);
				return (SizedBufferWithCap){ .capacity = 0 };
			}

			message_buffer = new_buffer;

			message_buffer[old_size + MESSAGE_BUF_SIZE] = '\0';

			++buffers_used;
			continue;
		}

		size_t total_size = (MESSAGE_BUF_SIZE * buffers_used) + bytes_read;

		// malloced, null terminated and probably "huge"
		return (SizedBufferWithCap){ .capacity = total_size,
			                         .result = (SizedBuffer){ .data = message_buffer,
			                                                  .size = total_size } };
	}
}

NODISCARD static SizedBuffer
read_buffer_from_connection_impl_deprecated(const ConnectionDescriptor* const descriptor) {
	// this buffer expands using realloc!!
	// also not the + 1 and the zero initialization, means that it's null terminated
	uint8_t* message_buffer = (uint8_t*)malloc(MESSAGE_BUF_SIZE + 1);

	if(!message_buffer) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	message_buffer[MESSAGE_BUF_SIZE] = '\0';

	size_t buffers_used = 0;
	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarios
		ReadResult read_result = read_from_descriptor(
		    descriptor, message_buffer + (MESSAGE_BUF_SIZE * buffers_used), MESSAGE_BUF_SIZE);

		if(read_result.type == ReadResultTypeError) {
			// NOTE. make a function that gets the error from openssl!
			LOG_MESSAGE(LogLevelWarn, "Couldn't read from a connection: %s\n",
			            get_read_error_meaning(descriptor, read_result.data.opaque_error));

			free(message_buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		if(read_result.type == ReadResultTypeEOF) {
			size_t total_size = (MESSAGE_BUF_SIZE * buffers_used);
			*(message_buffer + total_size) = '\0';

			// client disconnected, so done
			LOG_MESSAGE_SIMPLE(LogLevelTrace, "client disconnected\n");
			return (SizedBuffer){ .data = message_buffer, .size = total_size };
		}

		assert(read_result.type == ReadResultTypeSuccess);

		size_t bytes_read = read_result.data.bytes_read;

		if(bytes_read == MESSAGE_BUF_SIZE) {
			// now the buffer has to be reused, so it's re-allocated, the used realloc helper also
			// initializes it with 0 and copies the old content, so nothing is lost and a new
			// MESSAGE_BUF_SIZE capacity is available + a null byte at the end
			size_t old_size = ((buffers_used + 1) * MESSAGE_BUF_SIZE);
			uint8_t* new_buffer =
			    (uint8_t*)realloc(message_buffer, (old_size + MESSAGE_BUF_SIZE + 1));

			if(!new_buffer) {
				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
				                   "Couldn't re-allocate memory!\n");

				free(message_buffer);
				return (SizedBuffer){ .data = NULL, .size = 0 };
			}

			message_buffer = new_buffer;

			message_buffer[old_size + MESSAGE_BUF_SIZE] = '\0';

			++buffers_used;
			continue;
		}

		size_t total_size = (MESSAGE_BUF_SIZE * buffers_used) + bytes_read;

		*(message_buffer + total_size) = '\0';

		// malloced, null terminated and probably "huge"
		return (SizedBuffer){ .data = message_buffer, .size = total_size };
	}
}

char* read_string_from_connection_deprecated(const ConnectionDescriptor* const descriptor) {

	SizedBuffer value = read_buffer_from_connection_impl_deprecated(descriptor);

	if(value.data == NULL) {
		return NULL;
	}

	return value.data;
}

NODISCARD ReadTStrResult read_buffer_from_connection(const ConnectionDescriptor* descriptor) {

	SizedBufferWithCap value = read_buffer_from_connection_impl(descriptor);

	if(value.capacity == 0) {
		return (ReadTStrResult){ .is_error = true };
	}

	tstr str = tstr_own(value.result.data, value.result.size, value.capacity);

	return (ReadTStrResult){ .is_error = false, .data = { .str = str } };
}

NODISCARD ReadTStrResult read_tstr_from_connection(const ConnectionDescriptor* descriptor) {

	SizedBufferWithCap value = read_buffer_from_connection_impl(descriptor);

	if(value.capacity == 0) {
		return (ReadTStrResult){ .is_error = true };
	}

	tstr str = tstr_own(value.result.data, value.result.size, value.capacity);

	return (ReadTStrResult){ .is_error = false, .data = { .str = str } };
}
