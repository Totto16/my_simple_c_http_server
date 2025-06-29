

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

static bool constructHeadersForRequest(HttpResponse* response, const char* MIMEType,
                                       HttpHeaderFields additionalHeaders,
                                       COMPRESSION_TYPE compression_format) {

	STBDS_ARRAY_INIT(response->head.headerFields);

	// add standard fields

	{
		// MIME TYPE

		// add the standard ones, using %c with '\0' to use the trick, described above
		char* contentTypeBuffer = NULL;
		formatString(&contentTypeBuffer, return NULL;
		             , "%s%c%s", "Content-Type", '\0',
		             MIMEType == NULL ? DEFAULT_MIME_TYPE : MIMEType);

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		response->head.headerFields[current_array_index].key = contentTypeBuffer;
		response->head.headerFields[current_array_index].value =
		    contentTypeBuffer + strlen(contentTypeBuffer) + 1;
	}

	{
		// CONTENT LENGTH

		char* contentLengthBuffer = NULL;
		formatString(&contentLengthBuffer, return NULL;
		             , "%s%c%ld", "Content-Length", '\0', response->body.size);

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		response->head.headerFields[current_array_index].key = contentLengthBuffer;
		response->head.headerFields[current_array_index].value =
		    contentLengthBuffer + strlen(contentLengthBuffer) + 1;
	}

	{
		// Server

		char* serverBuffer = NULL;
		formatString(&serverBuffer, return NULL;
		             , "%s%c%s", "Server", '\0',
		             "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING));

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		response->head.headerFields[current_array_index].key = serverBuffer;
		response->head.headerFields[current_array_index].value =
		    serverBuffer + strlen(serverBuffer) + 1;
	}

	{

		// Content-Encoding

		if(compression_format != COMPRESSION_TYPE_NONE) {
			// add the standard ones, using %c with '\0' to use the trick, described above
			char* contentEncodingBuffer = NULL;
			formatString(&contentEncodingBuffer, return NULL;
			             , "%s%c%s", "Content-Encoding", '\0',
			             get_string_for_compress_format(compression_format));

			size_t current_array_index = stbds_arrlenu(response->head.headerFields);

			stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

			response->head.headerFields[current_array_index].key = contentEncodingBuffer;
			response->head.headerFields[current_array_index].value =
			    contentEncodingBuffer + strlen(contentEncodingBuffer) + 1;
		}
	}

	size_t current_array_size = stbds_arrlenu(response->head.headerFields);

	size_t headerSize = stbds_arrlenu(additionalHeaders);

	stbds_arrsetcap(response->head.headerFields, current_array_size + headerSize);

	for(size_t i = 0; i < headerSize; ++i) {

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		// ATTENTION; this things have to be ALL malloced
		response->head.headerFields[current_array_index].key = additionalHeaders[i].key;
		response->head.headerFields[current_array_index].value = additionalHeaders[i].value;
	}

	// if additional Headers are specified free them now
	if(headerSize > 0) {
		stbds_arrfree(additionalHeaders);
	}

	return true;
}

// simple http Response constructor using string builder, headers can be NULL, when headerSize is
// also null!
NODISCARD static HttpResponse* constructHttpResponse(HTTPResponseToSend toSend,
                                                     SendSettings send_settings) {

	HttpResponse* response = (HttpResponse*)mallocWithMemset(sizeof(HttpResponse), true);

	if(!response) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	// using the same trick as before, \0 in the malloced string :)
	const char* protocolVersion = "HTTP/1.1";
	size_t protocolLength = strlen(protocolVersion);
	const char* statusMessage = getStatusMessage(toSend.status);

	char* responseLineBuffer = NULL;
	formatString(&responseLineBuffer, return NULL;
	             , "%s%c%d%c%s", protocolVersion, '\0', toSend.status, '\0', statusMessage);

	response->head.responseLine.protocolVersion = responseLineBuffer;
	response->head.responseLine.statusCode = responseLineBuffer + protocolLength + 1;
	response->head.responseLine.statusMessage =
	    responseLineBuffer + protocolLength + strlen(responseLineBuffer + protocolLength + 1) + 2;

	COMPRESSION_TYPE format_used = send_settings.compression_to_use;

	if(toSend.body.body.data) {

		if(format_used != COMPRESSION_TYPE_NONE) {

			// here only supported protocols can be used, otherwise previous checks were wrong
			SizedBuffer new_body =
			    compress_buffer_with(toSend.body.body, send_settings.compression_to_use);

			if(!new_body.data) {
				LOG_MESSAGE(
				    LogLevelError,
				    "An error occured while compressing the body with the compression format %s\n",
				    get_string_for_compress_format(send_settings.compression_to_use));
				format_used = COMPRESSION_TYPE_NONE;
				response->body = toSend.body.body;
			} else {
				response->body = new_body;
				freeSizedBuffer(toSend.body.body);
			}
		} else {
			response->body = toSend.body.body;
		}
	} else {
		response->body = toSend.body.body;
		format_used = COMPRESSION_TYPE_NONE;
	}

	if(!constructHeadersForRequest(response, toSend.MIMEType, toSend.additionalHeaders,
	                               format_used)) {
		// TODO(Totto): free things accordingly
		return NULL;
	}

	// for that the body has to be malloced
	// finally retuning the malloced httpResponse
	return response;
}

NODISCARD static inline int sendMessageToConnection(const ConnectionDescriptor* descriptor,
                                                    HTTPResponseToSend toSend,
                                                    SendSettings send_settings) {

	HttpResponse* httpResponse = constructHttpResponse(toSend, send_settings);

	HttpConcattedResponse* concattedResponse = httpResponseConcat(httpResponse);

	if(!concattedResponse) {
		return -7;
	}

	int result = sendConcattedResponseToConnection(descriptor, concattedResponse);
	// body gets freed
	freeHttpResponse(httpResponse);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int sendHTTPMessageToConnection(const ConnectionDescriptor* const descriptor,
                                HTTPResponseToSend toSend, SendSettings send_settings) {

	return sendMessageToConnection(descriptor, toSend, send_settings);
}

static void httpResponseAdjustToRequestMethod(HTTPResponseToSend* responsePtr,
                                              HTTPRequestMethod method) {

	if(method == HTTPRequestMethodHead) {
		responsePtr->MIMEType = NULL;
		freeSizedBuffer(responsePtr->body.body);
		responsePtr->body = httpResponseBodyEmpty();
	}
}

NODISCARD int sendHTTPMessageToConnectionAdvanced(const ConnectionDescriptor* descriptor,
                                                  HTTPResponseToSend toSend,
                                                  SendSettings send_settings,
                                                  HttpRequestHead request_head) {

	httpResponseAdjustToRequestMethod(&toSend, request_head.requestLine.method);

	return sendHTTPMessageToConnection(descriptor, toSend, send_settings);
}

NODISCARD HTTPResponseBody httpResponseBodyFromStaticString(const char* static_string) {
	char* mallocedString = normalStringToMalloced(static_string);
	;

	return httpResponseBodyFromString(mallocedString);
}

NODISCARD HTTPResponseBody httpResponseBodyFromString(char* string) {
	return httpResponseBodyFromData(string, strlen(string));
}

NODISCARD HTTPResponseBody httpResponseBodyFromStringBuilder(StringBuilder* stringBuilder) {
	HTTPResponseBody result =
	    httpResponseBodyFromData(stringBuilder->data, stringBuilder->currentSize);
	free(stringBuilder);
	return result;
}

NODISCARD HTTPResponseBody httpResponseBodyFromData(void* data, size_t size) {
	return (HTTPResponseBody){ .body = (SizedBuffer){ .data = data, .size = size } };
}

NODISCARD HTTPResponseBody httpResponseBodyEmpty(void) {
	return (HTTPResponseBody){ .body = (SizedBuffer){ .data = NULL, .size = 0 } };
}
