

#include "./send.h"
#include "generic/send.h"

NODISCARD static int sendConcattedResponseToConnection(const ConnectionDescriptor* const descriptor,
                                                       HttpConcattedResponse* concattedResponse) {
	int result = sendStringBuilderToConnection(descriptor, concattedResponse->headers);
	if(result < 0) {
		return result;
	}

	if(concattedResponse->body.data) {
		result = sendSizedBufferToConnection(descriptor, concattedResponse->body);
	}

	free(concattedResponse);

	return result;
}

NODISCARD static int sendMessageToConnectionWithHeadersMalloced(
    const ConnectionDescriptor* const descriptor, int status, char* body, const char* MIMEType,
    HttpHeaderField* headerFields, const int headerFieldsAmount, SendSettings send_settings) {

	HttpResponse* message = constructHttpResponseWithHeaders(
	    status, body, headerFields, headerFieldsAmount, MIMEType, send_settings);

	HttpConcattedResponse* concattedResponse = httpResponseConcat(message);

	if(!concattedResponse) {
		return -7;
	}

	int result = sendConcattedResponseToConnection(descriptor, concattedResponse);
	// body gets freed
	freeHttpResponse(message);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
NODISCARD static int sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
                                                     int status, char* string_body,
                                                     const char* MIMEType,
                                                     SendSettings send_settings) {

	HttpResponse* message = constructHttpResponse(status, string_body, MIMEType, send_settings);

	HttpConcattedResponse* concattedResponse = httpResponseConcat(message);

	if(!concattedResponse) {
		return -7;
	}

	int result = sendConcattedResponseToConnection(descriptor, concattedResponse);
	// body gets freed
	freeHttpResponse(message);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int sendHTTPMessageToConnection(const ConnectionDescriptor* const descriptor, int status,
                                char* body, const char* MIMEType, HttpHeaderField* headerFields,
                                const int headerFieldsAmount, CONNECTION_SEND_FLAGS FLAGS,
                                SendSettings send_settings) {

	char* final_body = body;

	if((FLAGS & CONNECTION_SEND_FLAGS_UN_MALLOCED) != 0) {
		if(body) {
			final_body = normalStringToMalloced(body);
		}
	}

	if(headerFields == NULL || headerFieldsAmount == 0) {
		return sendMessageToConnectionMalloced(descriptor, status, final_body, MIMEType,
		                                       send_settings);
	}

	return sendMessageToConnectionWithHeadersMalloced(
	    descriptor, status, final_body, MIMEType, headerFields, headerFieldsAmount, send_settings);
}
