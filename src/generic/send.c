

#include "send.h"
#include "utils/log.h"

#include <errno.h>

int sendDataToConnection(const ConnectionDescriptor* const descriptor, void* toSend,
                         size_t length) {

	size_t remainingLength = length;

	size_t alreadyWritten = 0;
	// write bytes until all are written
	while(true) {
		ssize_t wroteBytes =
		    write_to_descriptor(descriptor, ((uint8_t*)toSend) + alreadyWritten, remainingLength);

		if(wroteBytes == -1) {
			LOG_MESSAGE(LogLevelError, "Couldn't write to a connection: %s\n", strerror(errno));
			return -errno;
		}

		if(wroteBytes == (ssize_t)remainingLength) {
			// the message was sent in one time
			break;
		}

		if(wroteBytes == 0) {
			/// shouldn't occur!
			LOG_MESSAGE(LogLevelError,
			            "FATAL: Write has an unsupported state: written %lu of %lu bytes\n",
			            alreadyWritten, length);
			return -2;
		}

		// otherwise repeat until that happened
		remainingLength -= wroteBytes;
		alreadyWritten += wroteBytes;
	}

	return 0;
}

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
int sendStringToConnection(const ConnectionDescriptor* const descriptor, char* toSend) {
	return sendDataToConnection(descriptor, toSend, strlen(toSend));
}

NODISCARD int sendSizedBufferToConnection(const ConnectionDescriptor* const descriptor,
                                          SizedBuffer buffer) {
	return sendDataToConnection(descriptor, buffer.data, buffer.size);
}

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
int sendStringBuilderToConnection(const ConnectionDescriptor* const descriptor,
                                  StringBuilder* stringBuilder) {
	int result =
	    sendSizedBufferToConnection(descriptor, string_builder_get_sized_buffer(stringBuilder));
	free_string_builder(stringBuilder);
	return result;
}
