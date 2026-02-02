
#include "read.h"

#include <errno.h>

#include "utils/log.h"

#define INITIAL_MESSAGE_BUF_SIZE 1024

NODISCARD static SizedBuffer
read_buffer_from_connection_impl(const ConnectionDescriptor* const descriptor) {
	// this buffer expands using realloc!!
	// also not the + 1 and the zero initialization, means that it's null terminated
	uint8_t* message_buffer = (uint8_t*)malloc(INITIAL_MESSAGE_BUF_SIZE + 1);

	if(!message_buffer) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	message_buffer[INITIAL_MESSAGE_BUF_SIZE] = '\0';

	size_t buffers_used = 0;
	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarios
		int read_bytes = read_from_descriptor(
		    descriptor, message_buffer + (INITIAL_MESSAGE_BUF_SIZE * buffers_used),
		    INITIAL_MESSAGE_BUF_SIZE);

		if(read_bytes == -1) {
			LOG_MESSAGE(LogLevelWarn, "Couldn't read from a connection: %s\n", strerror(errno));

			free(message_buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		if(read_bytes == 0) {
			size_t total_size = (INITIAL_MESSAGE_BUF_SIZE * buffers_used) + read_bytes;
			*(message_buffer + total_size) = '\0';

			// client disconnected, so done
			LOG_MESSAGE_SIMPLE(LogLevelTrace, "client disconnected\n");
			return (SizedBuffer){ .data = message_buffer, .size = total_size };
		}

		if(read_bytes == INITIAL_MESSAGE_BUF_SIZE) {
			// now the buffer has to be reused, so it's re-alloced, the used realloc helper also
			// initializes it with 0 and copies the old content, so nothing is lost and a new
			// INITIAL_MESSAGE_BUF_SIZE capacity is available + a null byte at the end
			size_t old_size = ((buffers_used + 1) * INITIAL_MESSAGE_BUF_SIZE);
			uint8_t* new_buffer =
			    (uint8_t*)realloc(message_buffer, (old_size + INITIAL_MESSAGE_BUF_SIZE + 1));

			if(!new_buffer) {
				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
				                   "Couldn't re-allocate memory!\n");

				free(message_buffer);
				return (SizedBuffer){ .data = NULL, .size = 0 };
			}

			message_buffer = new_buffer;

			message_buffer[old_size + INITIAL_MESSAGE_BUF_SIZE] = '\0';

			++buffers_used;
			continue;
		}

		size_t total_size = (INITIAL_MESSAGE_BUF_SIZE * buffers_used) + read_bytes;

		*(message_buffer + total_size) = '\0';

		// malloced, null terminated and probably "huge"
		return (SizedBuffer){ .data = message_buffer, .size = total_size };
	}
}

char* read_string_from_connection(const ConnectionDescriptor* const descriptor) {

	SizedBuffer value = read_buffer_from_connection_impl(descriptor);

	if(value.data == NULL) {
		return NULL;
	}

	return value.data;
}

NODISCARD SizedBuffer read_buffer_from_connection(const ConnectionDescriptor* descriptor) {
	return read_buffer_from_connection_impl(descriptor);
}

void* read_exact_bytes(const ConnectionDescriptor* const descriptor, size_t n_bytes) {

	void* message_buffer = malloc(n_bytes);

	if(!message_buffer) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return NULL;
	}

	size_t actual_bytes_read = 0;

	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarios
		int read_bytes =
		    read_from_descriptor(descriptor, ((uint8_t*)message_buffer) + actual_bytes_read,
		                         n_bytes - actual_bytes_read);

		if(read_bytes == -1) {
			LOG_MESSAGE(LogLevelWarn, "Couldn't read from a connection: %s\n", strerror(errno));
			return NULL;
		}

		if(read_bytes == 0) {
			if(n_bytes == actual_bytes_read) {
				return message_buffer;
			}

			// client disconnected too early, so it's an error
			LOG_MESSAGE_SIMPLE(LogLevelWarn, "EOF before all necessary bytes were read\n");
			return NULL;
		}

		actual_bytes_read += read_bytes;
	}

	// malloced, null terminated an probably "huge"
	return message_buffer;
}
