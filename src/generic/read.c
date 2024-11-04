
#include "read.h"

#include <errno.h>

#include "utils/log.h"

#define INITIAL_MESSAGE_BUF_SIZE 1024

char* readStringFromConnection(const ConnectionDescriptor* const descriptor) {
	// this buffer expands using realloc!!
	// also not the + 1 and the zero initialization, means that it's null terminated
	char* messageBuffer = (char*)malloc(INITIAL_MESSAGE_BUF_SIZE + 1);

	if(!messageBuffer) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	messageBuffer[INITIAL_MESSAGE_BUF_SIZE] = '\0';

	int buffersUsed = 0;
	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarios
		int readBytes = read_from_descriptor(
		    descriptor, messageBuffer + (INITIAL_MESSAGE_BUF_SIZE * buffersUsed),
		    INITIAL_MESSAGE_BUF_SIZE);
		if(readBytes == -1) {
			LOG_MESSAGE(LogLevelWarn, "Couldn't read from a connection: %s\n", strerror(errno));
			return NULL;
		} else if(readBytes == 0) {
			// client disconnected, so done
			break;
		} else if(readBytes == INITIAL_MESSAGE_BUF_SIZE) {
			// now the buffer has to be reused, so it's realloced, the used realloc helper also
			// initializes it with 0 and copies the old content, so nothing is lost and a new
			// INITIAL_MESSAGE_BUF_SIZE capacity is available + a null byte at the end
			size_t oldSize = ((buffersUsed + 1) * INITIAL_MESSAGE_BUF_SIZE) + 1;
			char* new_buffer = (char*)realloc(messageBuffer, oldSize + INITIAL_MESSAGE_BUF_SIZE);

			if(!new_buffer) {
				LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation,
				                   "Couldn't re-allocate memory!\n");
				return NULL;
			}
			messageBuffer = new_buffer;

			++buffersUsed;
		} else {
			// the message was shorter and could fit in the existing buffer!
			break;
		}
	}

	// malloced, null terminated an probably "huge"
	return messageBuffer;
}

char* readExactBytes(const ConnectionDescriptor* const descriptor, size_t n_bytes) {

	char* messageBuffer = (char*)malloc(n_bytes);

	if(!messageBuffer) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	size_t actualBytesRead = 0;

	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarios
		int readBytes = read_from_descriptor(descriptor, messageBuffer + actualBytesRead,
		                                     n_bytes - actualBytesRead);
		if(readBytes == -1) {
			LOG_MESSAGE(LogLevelWarn, "Couldn't read from a connection: %s\n", strerror(errno));
			return NULL;
		} else if(readBytes == 0) {
			if(n_bytes == actualBytesRead) {
				return messageBuffer;
			}

			// client disconnected too early, so it's an error
			LOG_MESSAGE_SIMPLE(LogLevelWarn, "EOF before all necessary bytes were read\n");
			return NULL;
		} else {
			actualBytesRead += readBytes;
		}
	}

	// malloced, null terminated an probably "huge"
	return messageBuffer;
}
