

#include "./send.h"
#include "generic/send.h"

NODISCARD static int
send_concatted_response_to_connection(const ConnectionDescriptor* const descriptor,
                                      HttpConcattedResponse* concatted_response) {
	int result = send_string_builder_to_connection(descriptor, &concatted_response->headers);
	if(result < 0) {
		return result;
	}

	if(concatted_response->body.data) {
		result = send_sized_buffer_to_connection(descriptor, concatted_response->body);
	}

	free(concatted_response);

	return result;
}

static bool construct_headers_for_request(HttpResponse* response, const char* mime_type,
                                          HttpHeaderFields additional_headers,
                                          CompressionType compression_format) {

	response->head.header_fields = STBDS_ARRAY_EMPTY;

	// add standard fields

	{
		// MIME TYPE

		// add the standard ones, using %c with '\0' to use the trick, described above
		char* content_type_buffer = NULL;
		FORMAT_STRING(&content_type_buffer, return NULL;
		              , "%s%c%s", "Content-Type", '\0',
		              mime_type == NULL ? DEFAULT_MIME_TYPE : mime_type);

		HttpHeaderField content_type_field = { .key = content_type_buffer,
			                                   .value = content_type_buffer +
			                                            strlen(content_type_buffer) + 1 };

		stbds_arrput(response->head.header_fields, content_type_field);
	}

	{
		// CONTENT LENGTH

		char* content_length_buffer = NULL;
		FORMAT_STRING(&content_length_buffer, return NULL;
		              , "%s%c%ld", "Content-Length", '\0', response->body.size);

		HttpHeaderField content_length_field = { .key = content_length_buffer,
			                                     .value = content_length_buffer +
			                                              strlen(content_length_buffer) + 1 };

		stbds_arrput(response->head.header_fields, content_length_field);
	}

	{
		// Server

		char* server_buffer = NULL;
		FORMAT_STRING(&server_buffer, return NULL;
		              , "%s%c%s", "Server", '\0',
		              "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING));

		HttpHeaderField server_field = { .key = server_buffer,
			                             .value = server_buffer + strlen(server_buffer) + 1 };

		stbds_arrput(response->head.header_fields, server_field);
	}

	{

		// Content-Encoding

		if(compression_format != CompressionTypeNone) {
			// add the standard ones, using %c with '\0' to use the trick, described above
			char* content_encoding_buffer = NULL;
			FORMAT_STRING(&content_encoding_buffer, return NULL;
			              , "%s%c%s", "Content-Encoding", '\0',
			              get_string_for_compress_format(compression_format));

			HttpHeaderField content_encoding_field = {
				.key = content_encoding_buffer,
				.value = content_encoding_buffer + strlen(content_encoding_buffer) + 1
			};

			stbds_arrput(response->head.header_fields, content_encoding_field);
		}
	}

	size_t current_array_size = stbds_arrlenu(response->head.header_fields);

	size_t header_size = stbds_arrlenu(additional_headers);

	stbds_arrsetcap(response->head.header_fields, current_array_size + header_size);

	for(size_t i = 0; i < header_size; ++i) {

		HttpHeaderField field = additional_headers[i];

		stbds_arrput(response->head.header_fields, field);
	}

	// if additional Headers are specified free them now
	if(header_size > 0) {
		stbds_arrfree(additional_headers);
	}

	return true;
}

// simple http Response constructor using string builder, headers can be NULL, when header_size is
// also null!
NODISCARD static HttpResponse* construct_http_response(HTTPResponseToSend to_send,
                                                       SendSettings send_settings) {

	HttpResponse* response = (HttpResponse*)malloc_with_memset(sizeof(HttpResponse), true);

	if(!response) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	// using the same trick as before, \0 in the malloced string :)
	const char* protocol_version = get_http_protocol_version_string(HTTPProtocolVersion1Dot1);
	size_t protocol_length = strlen(protocol_version);
	const char* status_message = get_status_message(to_send.status);

	char* response_line_buffer = NULL;
	FORMAT_STRING(&response_line_buffer, return NULL;
	              , "%s%c%d%c%s", protocol_version, '\0', to_send.status, '\0', status_message);

	response->head.response_line.protocol_version = response_line_buffer;
	response->head.response_line.status_code = response_line_buffer + protocol_length + 1;
	response->head.response_line.status_message =
	    response_line_buffer + protocol_length +
	    strlen(response_line_buffer + protocol_length + 1) + 2;

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

	if(!construct_headers_for_request(response, to_send.mime_type, to_send.additional_headers,
	                                  format_used)) {
		// TODO(Totto): free things accordingly
		return NULL;
	}

	if(!to_send.body.send_body_data) {
		free_sized_buffer(response->body);
		response->body = get_empty_sized_buffer();
	}

	// for that the body has to be malloced
	// finally retuning the malloced http_response
	return response;
}

NODISCARD static inline int send_message_to_connection(const ConnectionDescriptor* descriptor,
                                                       HTTPResponseToSend to_send,
                                                       SendSettings send_settings) {

	HttpResponse* http_response = construct_http_response(to_send, send_settings);

	HttpConcattedResponse* concatted_response = http_response_concat(http_response);

	if(!concatted_response) {
		// TODO(Totto): refactor error codes into an enum!
		return -7; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	int result = send_concatted_response_to_connection(descriptor, concatted_response);
	// body gets freed
	free_http_response(http_response);
	return result;
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int send_http_message_to_connection(const ConnectionDescriptor* const descriptor,
                                    HTTPResponseToSend to_send, SendSettings send_settings) {

	return send_message_to_connection(descriptor, to_send, send_settings);
}

NODISCARD int send_http_message_to_connection_advanced(const ConnectionDescriptor* descriptor,
                                                       HTTPResponseToSend to_send,
                                                       SendSettings send_settings,
                                                       HttpRequestHead request_head) {

	if(request_head.request_line.method == HTTPRequestMethodHead) {
		to_send.body.send_body_data = false;
	}

	return send_http_message_to_connection(descriptor, to_send, send_settings);
}

NODISCARD HTTPResponseBody http_response_body_from_static_string(const char* static_string) {
	char* malloced_string = strdup(static_string);

	return http_response_body_from_string(malloced_string);
}

NODISCARD HTTPResponseBody http_response_body_from_string(char* string) {
	return http_response_body_from_data(string, strlen(string));
}

NODISCARD HTTPResponseBody http_response_body_from_string_builder(StringBuilder** string_builder) {
	SizedBuffer string_builder_buffer = string_builder_release_into_sized_buffer(string_builder);
	HTTPResponseBody result =
	    http_response_body_from_data(string_builder_buffer.data, string_builder_buffer.size);
	return result;
}

NODISCARD HTTPResponseBody http_response_body_from_data(void* data, size_t size) {
	return (HTTPResponseBody){ .body = (SizedBuffer){ .data = data, .size = size },
		                       .send_body_data = true };
}

NODISCARD HTTPResponseBody http_response_body_empty(void) {
	return (HTTPResponseBody){ .body = get_empty_sized_buffer(), .send_body_data = true };
}
