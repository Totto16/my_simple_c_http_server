

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

		if(wroteBytes == 0) {
			/// shouldn't occur!
			LOG_MESSAGE_SIMPLE(LogLevelError, "FATAL: Write has an unsupported state!\n");
			return -2;
		}

		if(wroteBytes == (ssize_t)remainingLength) {
			// the message was sent in one time
			break;
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

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
int sendStringBuilderToConnection(const ConnectionDescriptor* const descriptor,
                                  StringBuilder* stringBuilder) {
	int result = sendStringToConnection(descriptor, string_builder_get_string(stringBuilder));
	string_builder_free(stringBuilder);
	return result;
}
