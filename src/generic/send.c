

#include "send.h"

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
void sendStringToConnection(const ConnectionDescriptor* const descriptor, char* toSend) {

	size_t remainingLength = strlen(toSend);

	int alreadyWritten = 0;
	// write bytes until all are written
	while(true) {
		ssize_t wroteBytes =
		    write_to_descriptor(descriptor, toSend + alreadyWritten, remainingLength);

		if(wroteBytes == -1) {
			// exit is a bit harsh, but atm there is no better error handling mechanism implemented,
			// that isn't necessary for that task
			perror("ERROR: Writing to a connection");
			exit(EXIT_FAILURE);
		} else if(wroteBytes == 0) {
			/// shouldn't occur!
			fprintf(stderr, "FATAL: Write has an unsupported state!\n");
			exit(EXIT_FAILURE);
		} else if(wroteBytes == (ssize_t)remainingLength) {
			// the message was sent in one time
			break;
		} else {
			// otherwise repeat until that happened
			remainingLength -= wroteBytes;
			alreadyWritten += wroteBytes;
		}
	}
}

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
void sendStringBuilderToConnection(const ConnectionDescriptor* const descriptor,
                                   StringBuilder* stringBuilder) {
	sendStringToConnection(descriptor, string_builder_get_string(stringBuilder));
	string_builder_free(stringBuilder);
}

static void sendMessageToConnectionWithHeadersMalloced(const ConnectionDescriptor* const descriptor,
                                                       int status, char* body, const char* MIMEType,
                                                       HttpHeaderField* headerFields,
                                                       const int headerFieldsAmount) {

	HttpResponse* message =
	    constructHttpResponseWithHeaders(status, body, headerFields, headerFieldsAmount, MIMEType);

	StringBuilder* messageString = httpResponseToStringBuilder(message);

	sendStringBuilderToConnection(descriptor, messageString);
	// body gets freed
	freeHttpResponse(message);
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
static void sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
                                            int status, char* body, const char* MIMEType) {

	HttpResponse* message = constructHttpResponse(status, body, MIMEType);

	StringBuilder* messageString = httpResponseToStringBuilder(message);

	sendStringBuilderToConnection(descriptor, messageString);
	// body gets freed
	freeHttpResponse(message);
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
void sendMessageToConnection(const ConnectionDescriptor* const descriptor, int status, char* body,
                             const char* MIMEType, HttpHeaderField* headerFields,
                             const int headerFieldsAmount, CONNECTION_SEND_FLAGS FLAGS) {

	char* final_body = body;

	if((FLAGS & CONNECTION_SEND_FLAGS_UN_MALLOCED) != 0) {
		if(body) {
			final_body = normalStringToMalloced(body);
		}
	}

	if(headerFields == NULL || headerFieldsAmount == 0) {
		sendMessageToConnectionMalloced(descriptor, status, final_body, MIMEType);
		return;
	}

	sendMessageToConnectionWithHeadersMalloced(descriptor, status, final_body, MIMEType,
	                                           headerFields, headerFieldsAmount);
	return;
}
