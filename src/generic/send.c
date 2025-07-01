

#include "send.h"
#include "utils/log.h"

#include <errno.h>

int send_data_to_connection(const ConnectionDescriptor* const descriptor, void* to_send,
                            size_t length) {

	size_t remaining_length = length;

	size_t already_written = 0;
	// write bytes until all are written
	while(true) {
		ssize_t wrote_bytes = write_to_descriptor(descriptor, ((uint8_t*)to_send) + already_written,
		                                          remaining_length);

		if(wrote_bytes == -1) {
			LOG_MESSAGE(LogLevelError, "Couldn't write to a connection: %s\n", strerror(errno));
			return -errno;
		}

		if(wrote_bytes == (ssize_t)remaining_length) {
			// the message was sent in one time
			break;
		}

		if(wrote_bytes == 0) {
			/// shouldn't occur!
			LOG_MESSAGE(LogLevelCritical,
			            "Write has an unsupported state: written %lu of %lu bytes\n",
			            already_written, length);
			return -2;
		}

		// otherwise repeat until that happened
		remaining_length -= wrote_bytes;
		already_written += wrote_bytes;
	}

	return 0;
}

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
int send_string_to_connection(const ConnectionDescriptor* const descriptor, char* to_send) {
	return send_data_to_connection(descriptor, to_send, strlen(to_send));
}

NODISCARD int send_sized_buffer_to_connection(const ConnectionDescriptor* const descriptor,
                                              SizedBuffer buffer) {
	return send_data_to_connection(descriptor, buffer.data, buffer.size);
}

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
int send_string_builder_to_connection(const ConnectionDescriptor* const descriptor,
                                      StringBuilder** string_builder) {

	SizedBuffer string_buffer = string_builder_release_into_sized_buffer(string_builder);

	int result = send_sized_buffer_to_connection(descriptor, string_buffer);
	free_sized_buffer(string_buffer);
	return result;
}
