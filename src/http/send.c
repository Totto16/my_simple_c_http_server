#include "./send.h"
#include "./parser.h"
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
		free(concatted_response);
		return result;
	}

	if(concatted_response->body.data != NULL) {
		result = send_sized_buffer_to_connection(descriptor, concatted_response->body);
	}

	free(concatted_response);

	return result;
}

static bool construct_http1_headers_for_request(
    SendSettings send_settings, HttpHeaderFields* const result_header_fields,
    const char* const mime_type, HttpHeaderFields additional_headers,
    CompressionType compression_format, const SizedBuffer body, const HttpStatusCode status) {

	// add standard fields

	{
		// MIME TYPE

		const char* const actual_mime_type = mime_type == NULL ? DEFAULT_MIME_TYPE : mime_type;

		add_http_header_field_const_key_const_value(
		    result_header_fields, HTTP_HEADER_NAME(content_type), actual_mime_type);
	}

	{
		if(send_settings.protocol_to_use != HTTPProtocolVersion2) {
			// CONTENT LENGTH

			char* content_length_buffer = NULL;
			FORMAT_STRING(&content_length_buffer, return NULL;, "%ld", body.size);

			add_http_header_field_const_key_dynamic_value(
			    result_header_fields, HTTP_HEADER_NAME(content_length), content_length_buffer);
		}
	}

	{

		// Eventual Connection header

		// TODO(Totto): once we support http1.1 keepalive, remove this

		if(send_settings.protocol_to_use != HTTPProtocolVersion2 &&
		   status != HttpStatusSwitchingProtocols) {

			add_http_header_field_const_key_const_value(result_header_fields,
			                                            HTTP_HEADER_NAME(connection), "close");
		}
		UNUSED(send_settings);
	}

	{
		// Server

		const char* const server_value = "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING);

		add_http_header_field_const_key_const_value(result_header_fields, HTTP_HEADER_NAME(server),
		                                            server_value);
	}

	{

		// Content-Encoding

		if(compression_format != CompressionTypeNone) {

			const char* const content_encoding = get_string_for_compress_format(compression_format);

			add_http_header_field_const_key_const_value(
			    result_header_fields, HTTP_HEADER_NAME(content_encoding), content_encoding);
		}
	}

	size_t current_array_size = TVEC_LENGTH(HttpHeaderField, *result_header_fields);

	size_t additional_headers_size = TVEC_LENGTH(HttpHeaderField, additional_headers);

	auto _ = TVEC_RESERVE(HttpHeaderField, result_header_fields,
	                      current_array_size + additional_headers_size);
	UNUSED(_);

	for(size_t i = 0; i < additional_headers_size; ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, additional_headers, i);

		auto _1 = TVEC_PUSH(HttpHeaderField, result_header_fields, field);
		UNUSED(_1);
	}

	// if additional Headers are specified free them now
	if(additional_headers_size > 0) {
		TVEC_FREE(HttpHeaderField, &additional_headers);
	}

	return true;
}

static bool construct_http2_headers_for_request(
    SendSettings send_settings, HttpHeaderFields* const result_header_fields,
    const char* const mime_type, HttpHeaderFields additional_headers,
    CompressionType compression_format, const SizedBuffer body, const HttpStatusCode status) {

	*result_header_fields = TVEC_EMPTY(HttpHeaderField);

	char* status_code_buffer = NULL;
	FORMAT_STRING(&status_code_buffer, return false;, "%u", status);

	add_http_header_field_const_key_dynamic_value(
	    result_header_fields, HTTP_HEADER_NAME(http2_pseudo_status), status_code_buffer);

	return construct_http1_headers_for_request(send_settings, result_header_fields, mime_type,
	                                           additional_headers, compression_format, body,
	                                           status);
}

typedef struct {
	SizedBuffer hpack_encoded_headers;
	SizedBuffer body;
} Http2Response;

NODISCARD static int send_http2_response_to_connection(const ConnectionDescriptor* const descriptor,

                                                       const Http2Response* const response,
                                                       HTTP2Context* const context) {

	bool headers_are_end_stream = response->body.data == NULL;

	Http2Identifier new_stream_identifier = get_new_http2_identifier(context);

	int result = http2_send_headers(descriptor, new_stream_identifier, context->settings,
	                                response->hpack_encoded_headers, headers_are_end_stream);

	if(result < 0) {
		return result;
	}

	if(response->body.data != NULL) {
		result =
		    http2_send_data(descriptor, new_stream_identifier, context->settings, response->body);
		if(result < 0) {
			return result;
		}
	}

	return 0;
}

typedef struct {
	HttpResponseHead head;
	SizedBuffer body;
} Http1Response;

// simple http Response constructor using string builder, headers can be NULL, when header_size is
// also null!
NODISCARD static Http1Response* construct_http1_response(HTTPResponseToSend to_send,
                                                         SendSettings send_settings) {

	Http1Response* response = (Http1Response*)malloc(sizeof(Http1Response));

	if(response == NULL) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return NULL;
	}

	*response = (Http1Response){ .head = {
		.header_fields = 	TVEC_EMPTY(HttpHeaderField),
		.response_line = {
			.protocol_version = NULL,.status_code = 0,
			.status_message = NULL,
		} 
	}, .body = (SizedBuffer){ .data = NULL, .size = 0 } };

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

	if(to_send.body.content.data) {

		if(format_used != CompressionTypeNone) {

			// here only supported protocols can be used, otherwise previous checks were wrong
			SizedBuffer new_body =
			    compress_buffer_with(to_send.body.content, send_settings.compression_to_use);

			if(!new_body.data) {
				LOG_MESSAGE(
				    LogLevelError,
				    "An error occurred while compressing the body with the compression format %s\n",
				    get_string_for_compress_format(send_settings.compression_to_use));
				format_used = CompressionTypeNone;
				response->body = to_send.body.content;
			} else {
				response->body = new_body;
				free_sized_buffer(to_send.body.content);
			}
		} else {
			response->body = to_send.body.content;
		}
	} else {
		response->body = to_send.body.content;
		format_used = CompressionTypeNone;
	}

	if(!construct_http1_headers_for_request(send_settings, &(response->head.header_fields),
	                                        to_send.mime_type, to_send.additional_headers,
	                                        format_used, response->body,to_send.status)) {
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

NODISCARD static Http2Response* construct_http2_response(Http2ContextState* const state,
                                                         HTTPResponseToSend to_send,
                                                         SendSettings send_settings) {

	Http2Response* response = (Http2Response*)malloc(sizeof(Http2Response));

	if(response == NULL) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return NULL;
	}

	*response = (Http2Response){ .hpack_encoded_headers = (SizedBuffer){ .data = NULL, .size = 0 },
		                         .body = (SizedBuffer){ .data = NULL, .size = 0 } };

	CompressionType format_used = send_settings.compression_to_use;

	if(to_send.body.content.data) {

		if(format_used != CompressionTypeNone) {

			// here only supported protocols can be used, otherwise previous checks were wrong
			SizedBuffer new_body =
			    compress_buffer_with(to_send.body.content, send_settings.compression_to_use);

			if(!new_body.data) {
				LOG_MESSAGE(
				    LogLevelError,
				    "An error occurred while compressing the body with the compression format %s\n",
				    get_string_for_compress_format(send_settings.compression_to_use));
				format_used = CompressionTypeNone;
				response->body = to_send.body.content;
			} else {
				response->body = new_body;
				free_sized_buffer(to_send.body.content);
			}
		} else {
			response->body = to_send.body.content;
		}
	} else {
		response->body = to_send.body.content;
		format_used = CompressionTypeNone;
	}

	HttpHeaderFields result_headers = TVEC_EMPTY(HttpHeaderField);

	if(!construct_http2_headers_for_request(send_settings, &result_headers, to_send.mime_type,
	                                        to_send.additional_headers, format_used, response->body,
	                                        to_send.status)) {
		// TODO(Totto): free things accordingly
		return NULL;
	}

	if(!to_send.body.send_body_data) {
		free_sized_buffer(response->body);
		response->body = get_empty_sized_buffer();
	}

	response->hpack_encoded_headers = http2_hpack_compress_data(state->hpack_state, result_headers);

	free_http_header_fields(&result_headers);

	if(response->hpack_encoded_headers.data == NULL) {
		return NULL;
	}

	// finally retuning the malloced http_response
	return response;
}

// makes a string_builder + a sized body from the HttpResponse, just does the opposite of parsing
// a Request, but with some slight modification
NODISCARD static Http1ConcattedResponse* http1_response_concat(Http1Response* response) {
	Http1ConcattedResponse* concatted_response =
	    (Http1ConcattedResponse*)malloc(sizeof(Http1ConcattedResponse));

	if(response == NULL) {
		return NULL;
	}

	if(concatted_response == NULL) {
		return NULL;
	}

	*concatted_response =
	    (Http1ConcattedResponse){ .headers = NULL,
		                          .body = (SizedBuffer){ .data = NULL, .size = 0 } };

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

static void free_http2_response(Http2Response* response) {
	free_sized_buffer(response->hpack_encoded_headers);
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

NODISCARD static inline int send_message_to_connection_http2(HTTP2Context* const context,
                                                             const ConnectionDescriptor* descriptor,
                                                             HTTPResponseToSend to_send,
                                                             SendSettings send_settings) {

	Http2Response* http_response =
	    construct_http2_response(&(context->state), to_send, send_settings);

	if(!http_response) {
		// TODO(Totto): refactor error codes into an enum!
		return -7; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	int result = send_http2_response_to_connection(descriptor, http_response, context);
	// body gets freed
	free_http2_response(http_response);
	return result;
}

NODISCARD static inline int send_message_to_connection(HTTPGeneralContext* const general_context,
                                                       const ConnectionDescriptor* descriptor,
                                                       HTTPResponseToSend to_send,
                                                       SendSettings send_settings) {

	if(send_settings.protocol_to_use == HTTPProtocolVersion2) {
		HTTP2Context* const context = http_general_context_get_http2_context(general_context);
		if(context == NULL) {
			return -143;
		}
		return send_message_to_connection_http2(context, descriptor, to_send, send_settings);
	}

	return send_message_to_connection_http1(descriptor, to_send, send_settings);
}

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
int send_http_message_to_connection(HTTPGeneralContext* const general_context,
                                    const ConnectionDescriptor* const descriptor,
                                    HTTPResponseToSend to_send, SendSettings send_settings) {

	return send_message_to_connection(general_context, descriptor, to_send, send_settings);
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
	return (HTTPResponseBody){ .content = (SizedBuffer){ .data = data, .size = size },
		                       .send_body_data = send_body };
}

NODISCARD HTTPResponseBody http_response_body_empty(void) {
	return (HTTPResponseBody){ .content = get_empty_sized_buffer(), .send_body_data = false };
}
