

#include "./send.h"
#include "generic/send.h"
#include "http/header.h"
#include "http/mime.h"

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

	response->head.header_fields = ZVEC_EMPTY(HttpHeaderField);

	// add standard fields

	{
		// MIME TYPE

		// add the standard ones, using %c with '\0' to use the trick, described above
		char* content_type_buffer = NULL;
		FORMAT_STRING(&content_type_buffer, return NULL;
		              , "%s%c%s", HTTP_HEADER_NAME(content_type), '\0',
		              mime_type == NULL ? DEFAULT_MIME_TYPE : mime_type);

		add_http_header_field_by_double_str(&response->head.header_fields, content_type_buffer);
	}

	{
		// CONTENT LENGTH

		char* content_length_buffer = NULL;
		FORMAT_STRING(&content_length_buffer, return NULL;
		              , "%s%c%ld", HTTP_HEADER_NAME(content_length), '\0', response->body.size);

		add_http_header_field_by_double_str(&response->head.header_fields, content_length_buffer);
	}

	{
		// Server

		char* server_buffer = NULL;
		FORMAT_STRING(&server_buffer, return NULL;
		              , "%s%c%s", HTTP_HEADER_NAME(server), '\0',
		              "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING));

		add_http_header_field_by_double_str(&response->head.header_fields, server_buffer);
	}

	{

		// Content-Encoding

		if(compression_format != CompressionTypeNone) {
			// add the standard ones, using %c with '\0' to use the trick, described above
			char* content_encoding_buffer = NULL;
			FORMAT_STRING(&content_encoding_buffer, return NULL;
			              , "%s%c%s", HTTP_HEADER_NAME(content_encoding), '\0',
			              get_string_for_compress_format(compression_format));

			add_http_header_field_by_double_str(&response->head.header_fields,
			                                    content_encoding_buffer);
		}
	}

	size_t current_array_size = ZVEC_LENGTH(response->head.header_fields);

	size_t additional_headers_size = ZVEC_LENGTH(additional_headers);

	auto _ = ZVEC_RESERVE(HttpHeaderField, &response->head.header_fields,
	                      current_array_size + additional_headers_size);
	UNUSED(_);

	for(size_t i = 0; i < additional_headers_size; ++i) {

		HttpHeaderField field = ZVEC_AT(HttpHeaderField, additional_headers, i);

		auto _1 = ZVEC_PUSH(HttpHeaderField, &response->head.header_fields, field);
		UNUSED(_1);
	}

	// if additional Headers are specified free them now
	if(additional_headers_size > 0) {
		ZVEC_FREE(HttpHeaderField, &additional_headers);
	}

	return true;
}

// simple http Response constructor using string builder, headers can be NULL, when header_size is
// also null!
NODISCARD static HttpResponse* construct_http_response(HTTPResponseToSend to_send,
                                                       SendSettings send_settings) {

	HttpResponse* response = (HttpResponse*)malloc_with_memset(sizeof(HttpResponse), true);

	if(!response) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return NULL;
	}

	// TODO(Totto): switch on to_send.protocol

	// using the same trick as before, \0 in the malloced string :)
	if(send_settings.protocol_to_use == HTTPProtocolVersionInvalid) {
		// FATAL error
		return NULL;
	}
	const char* protocol_version = get_http_protocol_version_string(send_settings.protocol_to_use);

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
				    "An error occurred while compressing the body with the compression format %s\n",
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
