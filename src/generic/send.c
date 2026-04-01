

#include "send.h"
#include "utils/log.h"

#include <errno.h>

GenericResult send_data_to_connection(const ConnectionDescriptor* const descriptor, void* to_send,
                                      size_t length) {

	size_t remaining_length = length;

	size_t already_written = 0;
	// write bytes until all are written
	while(true) {
		const ssize_t wrote_bytes = write_to_descriptor(
		    descriptor, (ReadonlyBuffer){ .data = ((uint8_t*)to_send) + already_written,
		                                  .size = remaining_length });

		if(wrote_bytes == -1) {
			LOG_MESSAGE(LogLevelError, "Couldn't write to a connection: %s\n", strerror(errno));
			// TODO(Totto): don't use strerror, as it uses an internal buffer, use better memory
			// management and maybe don't use the current locale!
			return GENERIC_RES_ERR_RAW(tstr_static_from_static_cstr(strerror(errno)));
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
			return GENERIC_RES_ERR_UNIQUE();
		}

		// otherwise repeat until that happened
		remaining_length -= wrote_bytes;
		already_written += wrote_bytes;
	}

	return GENERIC_RES_OK();
}

NODISCARD GenericResult send_buffer_to_connection(const ConnectionDescriptor* const descriptor,
                                                  SizedBuffer buffer) {
	return send_data_to_connection(descriptor, buffer.data, buffer.size);
}

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
GenericResult send_string_builder_to_connection(const ConnectionDescriptor* const descriptor,
                                                StringBuilder** string_builder) {

	const SizedBuffer string_buffer = string_builder_release_into_sized_buffer(string_builder);

	const GenericResult result = send_buffer_to_connection(descriptor, string_buffer);
	free_sized_buffer(string_buffer);
	return result;
}
