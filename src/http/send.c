

#include "./send.h"
#include "generic/send.h"

NODISCARD static int sendMessageToConnectionWithHeadersMalloced(
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
NODISCARD static int sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
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
int sendHTTPMessageToConnection(const ConnectionDescriptor* const descriptor, int status,
                                char* body, const char* MIMEType, HttpHeaderField* headerFields,
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
