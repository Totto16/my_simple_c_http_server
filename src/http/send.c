

#include "./send.h"
#include "generic/send.h"
#include "http/header.h"
#include "http/mime.h"
#include "http/v2.h"

typedef struct {
	StringBuilder* headers;
	SizedBuffer body;
} Http1ConcattedResponse;

NODISCARD static int
send_concatted_http1_response_to_connection(const ConnectionDescriptor* const descriptor,

                                            Http1ConcattedResponse* concatted_response) {
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

typedef struct {
	HttpResponseHead head;
	SizedBuffer body;
} Http1Response;

static bool construct_http1_headers_for_request(SendSettings send_settings, Http1Response* response,
                                                const char* mime_type,
                                                HttpHeaderFields additional_headers,
                                                CompressionType compression_format) {

	response->head.header_fields = TVEC_EMPTY(HttpHeaderField);

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

		// Eventual Connection header

		// TODO(Totto): once we support http1.1 keepalive, remove this

		/* if(send_settings.protocol_to_use != HTTPProtocolVersion2) {

		    char* connection_buffer = NULL;
		    FORMAT_STRING(&connection_buffer, return NULL;
		                  , "%s%c%s", HTTP_HEADER_NAME(connection), '\0', "close");

		    add_http_header_field_by_double_str(&response->head.header_fields, connection_buffer);
		} */
		UNUSED(send_settings);
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

	size_t current_array_size = TVEC_LENGTH(HttpHeaderField, response->head.header_fields);

	size_t additional_headers_size = TVEC_LENGTH(HttpHeaderField, additional_headers);

	auto _ = TVEC_RESERVE(HttpHeaderField, &response->head.header_fields,
	                      current_array_size + additional_headers_size);
	UNUSED(_);

	for(size_t i = 0; i < additional_headers_size; ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, additional_headers, i);

		auto _1 = TVEC_PUSH(HttpHeaderField, &response->head.header_fields, field);
		UNUSED(_1);
	}

	// if additional Headers are specified free them now
	if(additional_headers_size > 0) {
		TVEC_FREE(HttpHeaderField, &additional_headers);
	}

	return true;
}

// simple http Response constructor using string builder, headers can be NULL, when header_size is
// also null!
NODISCARD static Http1Response* construct_http1_response(HTTPResponseToSend to_send,
                                                         SendSettings send_settings) {

	Http1Response* response = (Http1Response*)malloc_with_memset(sizeof(Http1Response), true);

	if(response == NULL) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return NULL;
	}

	HTTPProtocolVersion version_to_use = send_settings.protocol_to_use;

	const char* protocol_version = get_http_protocol_version_string(version_to_use);

	size_t protocol_length = strlen(protocol_version);
	const char* status_message = get_status_message(to_send.status);

	// using the same trick as before, \0 in the malloced string :)
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

	if(!construct_http1_headers_for_request(send_settings, response, to_send.mime_type,
	                                        to_send.additional_headers, format_used)) {
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

// makes a string_builder + a sized body from the HttpResponse, just does the opposite of parsing
// a Request, but with some slight modification
NODISCARD static Http1ConcattedResponse* http1_response_concat(Http1Response* response) {
	Http1ConcattedResponse* concatted_response =
	    (Http1ConcattedResponse*)malloc_with_memset(sizeof(Http1ConcattedResponse), true);

	if(response == NULL) {
		return NULL;
	}

	if(concatted_response == NULL) {
		return NULL;
	}

	StringBuilder* result = string_builder_init();

	STRING_BUILDER_APPENDF(result, return NULL;
	                       , "%s %s %s%s", response->head.response_line.protocol_version,
	                       response->head.response_line.status_code,
	                       response->head.response_line.status_message, HTTP_LINE_SEPERATORS);

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, response->head.header_fields); ++i) {

		HttpHeaderField entry = TVEC_AT(HttpHeaderField, response->head.header_fields, i);

		STRING_BUILDER_APPENDF(result, return NULL;
		                       , "%s: %s%s", entry.key, entry.value, HTTP_LINE_SEPERATORS);
	}

	string_builder_append_single(result, HTTP_LINE_SEPERATORS);

	concatted_response->headers = result;
	concatted_response->body = response->body;

	return concatted_response;
}

// free the HttpResponse, just freeing everything necessary
static void free_http1_response(Http1Response* response) {
	// elegantly freeing three at once :)
	free(response->head.response_line.protocol_version);
	free_http_header_fields(&response->head.header_fields);

	free_sized_buffer(response->body);

	free(response);
}

NODISCARD static inline int send_message_to_connection_http1(const ConnectionDescriptor* descriptor,
                                                             HTTPResponseToSend to_send,
                                                             SendSettings send_settings) {

	Http1Response* http_response = construct_http1_response(to_send, send_settings);

	Http1ConcattedResponse* concatted_response = http1_response_concat(http_response);

	if(!concatted_response) {
		// TODO(Totto): refactor error codes into an enum!
		return -7; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	int result = send_concatted_http1_response_to_connection(descriptor, concatted_response);
	// body gets freed
	free_http1_response(http_response);
	return result;
}

NODISCARD static inline int send_message_to_connection_http2(const ConnectionDescriptor* descriptor,
                                                             HTTPResponseToSend to_send,
                                                             SendSettings send_settings) {

	// TODO: send proper http2 response

	Http1Response* http_response = construct_http1_response(to_send, send_settings);

	Http1ConcattedResponse* concatted_response = http1_response_concat(http_response);

	if(!concatted_response) {
		// TODO(Totto): refactor error codes into an enum!
		return -7; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	SizedBuffer header_buffer =
	    string_builder_release_into_sized_buffer(&concatted_response->headers);

	size_t length = header_buffer.size + concatted_response->body.size;

	SizedBuffer debug_data = allocate_sized_buffer(length);

	if(debug_data.data == NULL) {
		return -11;
	}

	memcpy(debug_data.data, header_buffer.data, header_buffer.size);
	memcpy(((uint8_t*)debug_data.data) + header_buffer.size, concatted_response->body.data,
	       concatted_response->body.size);

	int result =
	    http2_send_stream_error_with_data(descriptor, Http2ErrorCodeInternalError, debug_data);

	free_sized_buffer(header_buffer);
	free_http1_response(http_response);
	return result;
}

NODISCARD static inline int send_message_to_connection(const ConnectionDescriptor* descriptor,
                                                       HTTPResponseToSend to_send,
                                                       SendSettings send_settings) {

	if(send_settings.protocol_to_use == HTTPProtocolVersion2) {
		return send_message_to_connection_http2(descriptor, to_send, send_settings);
	}

	return send_message_to_connection_http1(descriptor, to_send, send_settings);
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int send_http_message_to_connection(const ConnectionDescriptor* const descriptor,
                                    HTTPResponseToSend to_send, SendSettings send_settings) {

	return send_message_to_connection(descriptor, to_send, send_settings);
}

NODISCARD HTTPResponseBody http_response_body_from_static_string(const char* static_string,
                                                                 bool send_body) {
	char* malloced_string = strdup(static_string);

	return http_response_body_from_string(malloced_string, send_body);
}

NODISCARD HTTPResponseBody http_response_body_from_string(char* string, bool send_body) {
	return http_response_body_from_data(string, strlen(string), send_body);
}

NODISCARD HTTPResponseBody http_response_body_from_string_builder(StringBuilder** string_builder,
                                                                  bool send_body) {
	SizedBuffer string_builder_buffer = string_builder_release_into_sized_buffer(string_builder);
	HTTPResponseBody result = http_response_body_from_data(string_builder_buffer.data,
	                                                       string_builder_buffer.size, send_body);
	return result;
}

NODISCARD HTTPResponseBody http_response_body_from_data(void* data, size_t size, bool send_body) {
	return (HTTPResponseBody){ .body = (SizedBuffer){ .data = data, .size = size },
		                       .send_body_data = send_body };
}

NODISCARD HTTPResponseBody http_response_body_empty(void) {
	return (HTTPResponseBody){ .body = get_empty_sized_buffer(), .send_body_data = false };
}
