

#include "send.h"
#include "utils/log.h"

bool sendDataToConnection(const ConnectionDescriptor* const descriptor, void* toSend,
                          size_t length) {

	size_t remainingLength = length;

	int alreadyWritten = 0;
	// write bytes until all are written
	while(true) {
		ssize_t wroteBytes =
		    write_to_descriptor(descriptor, ((uint8_t*)toSend) + alreadyWritten, remainingLength);

		if(wroteBytes == -1) {
			LOG_MESSAGE(LogLevelError, "Couldn't write to a connection: %s\n", strerror(errno));
			return false;
		} else if(wroteBytes == 0) {
			/// shouldn't occur!
			fprintf(stderr, "FATAL: Write has an unsupported state!\n");
			return false;
		} else if(wroteBytes == (ssize_t)remainingLength) {
			// the message was sent in one time
			break;
		} else {
			// otherwise repeat until that happened
			remainingLength -= wroteBytes;
			alreadyWritten += wroteBytes;
		}
	}

	return true;
}

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
bool sendStringToConnection(const ConnectionDescriptor* const descriptor, char* toSend) {
	return sendDataToConnection(descriptor, toSend, strlen(toSend));
}

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
bool sendStringBuilderToConnection(const ConnectionDescriptor* const descriptor,
                                   StringBuilder* stringBuilder) {
	bool result = sendStringToConnection(descriptor, string_builder_get_string(stringBuilder));
	string_builder_free(stringBuilder);
	return result;
}

static NODISCARD bool sendMessageToConnectionWithHeadersMalloced(
    const ConnectionDescriptor* const descriptor, int status, char* body, const char* MIMEType,
    HttpHeaderField* headerFields, const int headerFieldsAmount) {

	HttpResponse* message =
	    constructHttpResponseWithHeaders(status, body, headerFields, headerFieldsAmount, MIMEType);

	StringBuilder* messageString = httpResponseToStringBuilder(message);

	bool result = sendStringBuilderToConnection(descriptor, messageString);
	// body gets freed
	freeHttpResponse(message);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
static NODISCARD bool sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
                                                      int status, char* body,
                                                      const char* MIMEType) {

	HttpResponse* message = constructHttpResponse(status, body, MIMEType);

	StringBuilder* messageString = httpResponseToStringBuilder(message);

	bool result = sendStringBuilderToConnection(descriptor, messageString);
	// body gets freed
	freeHttpResponse(message);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
bool sendMessageToConnection(const ConnectionDescriptor* const descriptor, int status, char* body,
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
