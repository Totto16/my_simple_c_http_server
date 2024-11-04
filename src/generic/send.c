

#include "send.h"
#include "utils/log.h"

int sendDataToConnection(const ConnectionDescriptor* const descriptor, void* toSend,
                         size_t length) {

	size_t remainingLength = length;

	int alreadyWritten = 0;
	// write bytes until all are written
	while(true) {
		ssize_t wroteBytes =
		    write_to_descriptor(descriptor, ((uint8_t*)toSend) + alreadyWritten, remainingLength);

		if(wroteBytes == -1) {
			LOG_MESSAGE(LogLevelError, "Couldn't write to a connection: %s\n", strerror(errno));
			return -1;
		} else if(wroteBytes == 0) {
			/// shouldn't occur!
			LOG_MESSAGE_SIMPLE(LogLevelError, "FATAL: Write has an unsupported state!\n");
			return -2;
		} else if(wroteBytes == (ssize_t)remainingLength) {
			// the message was sent in one time
			break;
		} else {
			// otherwise repeat until that happened
			remainingLength -= wroteBytes;
			alreadyWritten += wroteBytes;
		}
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

static NODISCARD bool sendMessageToConnectionWithHeadersMalloced(
    const ConnectionDescriptor* const descriptor, int status, char* body, const char* MIMEType,
    HttpHeaderField* headerFields, const int headerFieldsAmount) {

	HttpResponse* message =
	    constructHttpResponseWithHeaders(status, body, headerFields, headerFieldsAmount, MIMEType);

	StringBuilder* messageString = httpResponseToStringBuilder(message);

	int result = sendStringBuilderToConnection(descriptor, messageString);
	// body gets freed
	freeHttpResponse(message);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
static NODISCARD int sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
                                                     int status, char* body, const char* MIMEType) {

	HttpResponse* message = constructHttpResponse(status, body, MIMEType);

	StringBuilder* messageString = httpResponseToStringBuilder(message);

	int result = sendStringBuilderToConnection(descriptor, messageString);
	// body gets freed
	freeHttpResponse(message);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int sendMessageToConnection(const ConnectionDescriptor* const descriptor, int status, char* body,
                            const char* MIMEType, HttpHeaderField* headerFields,
                            const int headerFieldsAmount, CONNECTION_SEND_FLAGS FLAGS) {

	char* final_body = body;

	if((FLAGS & CONNECTION_SEND_FLAGS_UN_MALLOCED) != 0) {
		if(body) {
			final_body = normalStringToMalloced(body);
		}
	}

	if(headerFields == NULL || headerFieldsAmount == 0) {
		return sendMessageToConnectionMalloced(descriptor, status, final_body, MIMEType);
	}

	return sendMessageToConnectionWithHeadersMalloced(descriptor, status, final_body, MIMEType,
	                                                  headerFields, headerFieldsAmount);
}
