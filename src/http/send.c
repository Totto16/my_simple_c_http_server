

#include "./send.h"
#include "generic/send.h"

NODISCARD static int sendConcattedResponseToConnection(const ConnectionDescriptor* const descriptor,
                                                       HttpConcattedResponse* concattedResponse) {
	int result = send_string_builder_to_connection(descriptor, &concattedResponse->headers);
	if(result < 0) {
		return result;
	}

	if(concattedResponse->body.data) {
		result = send_sized_buffer_to_connection(descriptor, concattedResponse->body);
	}

	free(concattedResponse);

	return result;
}

static bool constructHeadersForRequest(HttpResponse* response, const char* MIMEType,
                                       HttpHeaderFields additional_headers,
                                       CompressionType compression_format) {

	STBDS_ARRAY_INIT(response->head.header_fields);

	// add standard fields

	{
		// MIME TYPE

		// add the standard ones, using %c with '\0' to use the trick, described above
		char* contentTypeBuffer = NULL;
		FORMAT_STRING(&contentTypeBuffer, return NULL;
		              , "%s%c%s", "Content-Type", '\0',
		              MIMEType == NULL ? DEFAULT_MIME_TYPE : MIMEType);

		size_t current_array_index = stbds_arrlenu(response->head.header_fields);

		stbds_arrsetlen(response->head.header_fields, current_array_index + 1);

		response->head.header_fields[current_array_index].key = contentTypeBuffer;
		response->head.header_fields[current_array_index].value =
		    contentTypeBuffer + strlen(contentTypeBuffer) + 1;
	}

	{
		// CONTENT LENGTH

		char* contentLengthBuffer = NULL;
		FORMAT_STRING(&contentLengthBuffer, return NULL;
		              , "%s%c%ld", "Content-Length", '\0', response->body.size);

		size_t current_array_index = stbds_arrlenu(response->head.header_fields);

		stbds_arrsetlen(response->head.header_fields, current_array_index + 1);

		response->head.header_fields[current_array_index].key = contentLengthBuffer;
		response->head.header_fields[current_array_index].value =
		    contentLengthBuffer + strlen(contentLengthBuffer) + 1;
	}

	{
		// Server

		char* serverBuffer = NULL;
		FORMAT_STRING(&serverBuffer, return NULL;
		              , "%s%c%s", "Server", '\0',
		              "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING));

		size_t current_array_index = stbds_arrlenu(response->head.header_fields);

		stbds_arrsetlen(response->head.header_fields, current_array_index + 1);

		response->head.header_fields[current_array_index].key = serverBuffer;
		response->head.header_fields[current_array_index].value =
		    serverBuffer + strlen(serverBuffer) + 1;
	}

	{

		// Content-Encoding

		if(compression_format != CompressionTypeNone) {
			// add the standard ones, using %c with '\0' to use the trick, described above
			char* contentEncodingBuffer = NULL;
			FORMAT_STRING(&contentEncodingBuffer, return NULL;
			              , "%s%c%s", "Content-Encoding", '\0',
			              get_string_for_compress_format(compression_format));

			size_t current_array_index = stbds_arrlenu(response->head.header_fields);

			stbds_arrsetlen(response->head.header_fields, current_array_index + 1);

			response->head.header_fields[current_array_index].key = contentEncodingBuffer;
			response->head.header_fields[current_array_index].value =
			    contentEncodingBuffer + strlen(contentEncodingBuffer) + 1;
		}
	}

	size_t current_array_size = stbds_arrlenu(response->head.header_fields);

	size_t headerSize = stbds_arrlenu(additional_headers);

	stbds_arrsetcap(response->head.header_fields, current_array_size + headerSize);

	for(size_t i = 0; i < headerSize; ++i) {

		size_t current_array_index = stbds_arrlenu(response->head.header_fields);

		stbds_arrsetlen(response->head.header_fields, current_array_index + 1);

		// ATTENTION; this things have to be ALL malloced
		response->head.header_fields[current_array_index].key = additional_headers[i].key;
		response->head.header_fields[current_array_index].value = additional_headers[i].value;
	}

	// if additional Headers are specified free them now
	if(headerSize > 0) {
		stbds_arrfree(additional_headers);
	}

	return true;
}

// simple http Response constructor using string builder, headers can be NULL, when headerSize is
// also null!
NODISCARD static HttpResponse* constructHttpResponse(HTTPResponseToSend to_send,
                                                     SendSettings send_settings) {

	HttpResponse* response = (HttpResponse*)malloc_with_memset(sizeof(HttpResponse), true);

	if(!response) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	// using the same trick as before, \0 in the malloced string :)
	const char* protocol_version = "HTTP/1.1";
	size_t protocolLength = strlen(protocol_version);
	const char* statusMessage = get_status_message(to_send.status);

	char* response_line_buffer = NULL;
	FORMAT_STRING(&response_line_buffer, return NULL;
	              , "%s%c%d%c%s", protocol_version, '\0', to_send.status, '\0', statusMessage);

	response->head.response_line.protocol_version = response_line_buffer;
	response->head.response_line.status_code = response_line_buffer + protocolLength + 1;
	response->head.response_line.status_message =
	    response_line_buffer + protocolLength + strlen(response_line_buffer + protocolLength + 1) +
	    2;

	CompressionType format_used = send_settings.compression_to_use;

	if(to_send.body.body.data) {

		if(format_used != CompressionTypeNone) {

			// here only supported protocols can be used, otherwise previous checks were wrong
			SizedBuffer new_body =
			    compress_buffer_with(to_send.body.body, send_settings.compression_to_use);

			if(!new_body.data) {
				LOG_MESSAGE(
				    LogLevelError,
				    "An error occured while compressing the body with the compression format %s\n",
				    get_string_for_compress_format(send_settings.compression_to_use));
				format_used = CompressionTypeNone;
				response->body = to_send.body.body;
			} else {
				response->body = new_body;
				free_sized_buffer(to_send.body.body);
			}
		} else {
			response->body = to_send.body.body;
		}
	} else {
		response->body = to_send.body.body;
		format_used = CompressionTypeNone;
	}

	if(!constructHeadersForRequest(response, to_send.mime_type, to_send.additional_headers,
	                               format_used)) {
		// TODO(Totto): free things accordingly
		return NULL;
	}

	if(!to_send.body.sendBodyData) {
		free_sized_buffer(response->body);
		response->body = get_empty_sized_buffer();
	}

	// for that the body has to be malloced
	// finally retuning the malloced httpResponse
	return response;
}

NODISCARD static inline int sendMessageToConnection(const ConnectionDescriptor* descriptor,
                                                    HTTPResponseToSend to_send,
                                                    SendSettings send_settings) {

	HttpResponse* httpResponse = constructHttpResponse(to_send, send_settings);

	HttpConcattedResponse* concattedResponse = http_response_concat(httpResponse);

	if(!concattedResponse) {
		return -7;
	}

	int result = sendConcattedResponseToConnection(descriptor, concattedResponse);
	// body gets freed
	free_http_response(httpResponse);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int sendHTTPMessageToConnection(const ConnectionDescriptor* const descriptor,
                                HTTPResponseToSend to_send, SendSettings send_settings) {

	return sendMessageToConnection(descriptor, to_send, send_settings);
}

NODISCARD int sendHTTPMessageToConnectionAdvanced(const ConnectionDescriptor* descriptor,
                                                  HTTPResponseToSend to_send,
                                                  SendSettings send_settings,
                                                  HttpRequestHead request_head) {

	if(request_head.request_line.method == HTTPRequestMethodHead) {
		to_send.body.sendBodyData = false;
	}

	return sendHTTPMessageToConnection(descriptor, to_send, send_settings);
}

NODISCARD HTTPResponseBody httpResponseBodyFromStaticString(const char* static_string) {
	char* mallocedString = strdup(static_string);

	return httpResponseBodyFromString(mallocedString);
}

NODISCARD HTTPResponseBody httpResponseBodyFromString(char* string) {
	return httpResponseBodyFromData(string, strlen(string));
}

NODISCARD HTTPResponseBody httpResponseBodyFromStringBuilder(StringBuilder** string_builder) {
	SizedBuffer string_builder_buffer = string_builder_release_into_sized_buffer(string_builder);
	HTTPResponseBody result =
	    httpResponseBodyFromData(string_builder_buffer.data, string_builder_buffer.size);
	return result;
}

NODISCARD HTTPResponseBody httpResponseBodyFromData(void* data, size_t size) {
	return (HTTPResponseBody){ .body = (SizedBuffer){ .data = data, .size = size },
		                       .sendBodyData = true };
}

NODISCARD HTTPResponseBody httpResponseBodyEmpty(void) {
	return (HTTPResponseBody){ .body = get_empty_sized_buffer(), .sendBodyData = true };
}
