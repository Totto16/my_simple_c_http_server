
#include "./parser.h"
#include "./header.h"
#include "./v2.h"
#include "generic/read.h"
#include "utils/parse.h"

#include <ctype.h>

NODISCARD static HTTPRequestMethod get_http_method_from_string(const char* const method,
                                                               OUT_PARAM(bool) success) {

	if(strcmp(method, "GET") == 0) {
		*success = true;
		return HTTPRequestMethodGet;
	}

	if(strcmp(method, "POST") == 0) {
		*success = true;
		return HTTPRequestMethodPost;
	}

	if(strcmp(method, "HEAD") == 0) {
		*success = true;
		return HTTPRequestMethodHead;
	}

	if(strcmp(method, "OPTIONS") == 0) {
		*success = true;
		return HTTPRequestMethodOptions;
	}

	if(strcmp(method, "CONNECT") == 0) {
		*success = true;
		return HTTPRequestMethodConnect;
	}

	if(strcmp(method, "PRI") == 0) {
		*success = true;
		return HTTPRequestMethodPRI;
	}

	*success = false;
	return HTTPRequestMethodGet;
}

NODISCARD static HTTPProtocolVersion get_protocol_version_from_string(const char* protocol_version,
                                                                      OUT_PARAM(bool) success) {

	if(strcmp(protocol_version, "HTTP/1.0") == 0) {
		*success = true;
		return HTTPProtocolVersion1;
	}

	if(strcmp(protocol_version, "HTTP/1.1") == 0) {
		*success = true;
		return HTTPProtocolVersion1Dot1;
	}

	if(strcmp(protocol_version, "HTTP/2") == 0) {
		*success = true;
		return HTTPProtocolVersion2;
	}

	*success = false;
	return HTTPProtocolVersion1;
}

#define HTTP_LINE_SEPERATORS "\r\n"

#define SIZEOF_HTTP_LINE_SEPERATORS 2

static_assert((sizeof(HTTP_LINE_SEPERATORS) / (sizeof(HTTP_LINE_SEPERATORS[0]))) - 1 ==
              SIZEOF_HTTP_LINE_SEPERATORS);

// parse in string form, for http 1
NODISCARD static HttpRequestLine parse_http1_request_line(ParseState* state,
                                                          OUT_PARAM(bool) success) {

	HttpRequestLine result = {};

	char* const start = (char*)state->data.data + state->cursor;

	Byte* line_end = parser_get_until_delimiter(state, HTTP_LINE_SEPERATORS);

	if(line_end == NULL) {
		*success = false;
		return result;
	}

	// TODO: don't use libc parse function that operate on strings, use parser ones, that operate on
	// slices of bytes

	// make this string parseable by the libc functions
	*line_end = '\0';

	char* method = NULL;
	char* path = NULL;
	char* protocol_version = NULL;

	{ // parse the three filed in the line from start to line_end

		char* method_end = strchr(start, ' ');

		if(method_end == NULL) {
			*success = false;
			return result;
		}

		*method_end = '\0';

		method = start;

		path = method_end + 1;

		char* path_end = strchr(path, ' ');

		if(path_end == NULL) {
			*success = false;
			return result;
		}

		*path_end = '\0';

		protocol_version = path_end + 1;
	}

	ParsedRequestURIResult uri_result = parse_request_uri(path);

	if(uri_result.is_error) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		            "Invalid uri in HTTP request: %s\n", uri_result.value.error);
		// TODO: use zerror!
		*success = false;
		return result;
	}

	result.uri = uri_result.value.uri;

	result.method = get_http_method_from_string(method, success);

	if(!(*success)) {
		return result;
	}

	result.protocol_version = get_protocol_version_from_string(protocol_version, success);

	return result;
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPReaderStateEmpty = 0,
	HTTPReaderStateReading,
	HTTPReaderStateEnd,
	HTTPReaderStateError,
} HTTPReaderState;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPContentTypeV1 = 0,
	HTTPContentTypeV1Keepalive,
	HTTPContentTypeV2,
} HTTPContentType;

typedef struct {
	HTTPContentType type;
	union {
		int todo;
	} data;
} HTTPContent;

struct HTTPReaderImpl {
	ConnectionDescriptor* descriptor;
	ProtocolSelected protocol;
	//
	HTTPReaderState state;
	ParseState parser;
	HTTPContent content;
};

NODISCARD HTTPReader* NULLABLE
initialize_http_reader_from_connection(ConnectionDescriptor* const descriptor) {

	HTTPReader* reader = malloc(sizeof(HTTPReader));

	if(!reader) {
		return NULL;
	}

	ProtocolSelected protocol = get_selected_protocol(descriptor);

	*reader = (HTTPReader){
		.descriptor = descriptor,
		.protocol = protocol,
		.state = HTTPReaderStateEmpty,
		.parser =
		    (ParseState){
		        .cursor = 0,
		        .data = (SizedBuffer){ .data = NULL, .size = 0 },
		    },
		.content = { .type = HTTPContentTypeV1 },
	};

	return reader;
}

NODISCARD static HttpRequestResult parse_http2_request(HTTPReader* const reader) {
	UNUSED(reader);
	UNREACHABLE();
}

NODISCARD static HttpRequestResult parse_http1_request(const HttpRequestLine request_line,
                                                       HTTPReader* const reader) {

	HttpRequest result = {
		.head = { .request_line = request_line, .header_fields = ZVEC_EMPTY(HttpHeaderField) },
		.body = (SizedBuffer){ .data = NULL, .size = 0 },
	};

	ParseState* state = &(reader->parser);

	char* const start = (char*)state->data.data + state->cursor;

	// this can be changed by calls to parser_get_until_delimiter
	volatile size_t* cursor = &(state->cursor);

	// TODO: don't use libc parse function that operate on strings, use parser ones, that operate on
	// slices of bytes

	bool headers_finished = false;

	char* current_pos = start;

	// parse headers
	do {

		Byte* line_end = parser_get_until_delimiter(state, HTTP_LINE_SEPERATORS);

		if(line_end == NULL) {
			return (
			    HttpRequestResult){ .is_error = true,
				                    .value = { .error = (HttpRequestError){
				                                   .is_advanced = true,
				                                   .value = { .advanced =
				                                                  "Failed to parse headers" } } } };
		}

		*line_end = '\0';

		if(strlen(current_pos) == 0) {
			headers_finished = true;
		}

		char* header_start = current_pos;
		current_pos = (char*)state->data.data + state->cursor;

		{ // parse the single header

			char* header_key_end = strchr(header_start, ':');

			if(header_key_end == NULL) {
				return (HttpRequestResult){
					.is_error = true,
					.value = { .error =
					               (HttpRequestError){
					                   .is_advanced = true,
					                   .value = { .advanced = "Failed to parse header key" } } }
				};
			}

			*header_key_end = '\0';

			char* header_value_start = header_key_end + 1;

			while(isspace(*header_value_start)) {
				header_value_start = header_value_start + 1;
			}

			HttpHeaderField field = { .key = strdup(header_start),
				                      .value = strdup(header_value_start) };

			auto _ = ZVEC_PUSH(HttpHeaderField, &(result.head.header_fields), field);
			UNUSED(_);
		}

	} while(!headers_finished);

	// TODO: ws / compression and other headers get analyzed
	const auto TODO_request_settings = http_analyze_headers(result.head.header_fields);

	// TODO: support more body encodings
	const auto TODO_body = parse_body(RequestSettings, reader);

	result.body = TODO_body;

	// check if the request body makes sense
	if((request_line.method == HTTPRequestMethodGet ||
	    request_line.method == HTTPRequestMethodHead ||
	    request_line.method == HTTPRequestMethodOptions) &&
	   result.body.size != 0) {

		return (HttpRequestResult){
			.is_error = true,
			.value = { .error =
			               (HttpRequestError){
			                   .is_advanced = false,
			                   .value = { .enum_value =
			                                  HttpRequestErrorTypeInvalidNonEmptyBody } } }
		};
	}
}

NODISCARD static HttpRequestResult parse_first_http_request(HTTPReader* const reader) {

	bool success = false;

	// only possible in the first request, as also http2 must send a valid http1 request line (the
	// preface)
	HttpRequestLine request_line = parse_http1_request_line(&(reader->parser), &success);

	if(!success) {
		return (HttpRequestResult){
			.is_error = true,
			.value = { .error =
			               (HttpRequestError){
			                   .is_advanced = true,
			                   .value = { .advanced = "failed to parse request line" } } }
		};
	}

	{ // check for logic errors, this differs in the first request and the following ones, as the
	  // first request might negotiate some things

		// check if the negotiated protocol matches!
		switch(reader->protocol) {
			case ProtocolSelectedHttp1Dot1: {

				if(request_line.protocol_version == HTTPProtocolVersion2) {
					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = false,
						                   .value = { .enum_value =
						                                  HttpRequestErrorTypeInvalidHttpVersion } } }
					};
				}

				break;
			}
			case ProtocolSelectedHttp2: {

				// this consumes more bytes, if the request line matches, otherwise it is an error
				// if it matches the data after the reader cursors position is already the http2
				// request!
				const Http2PrefaceStatus http2_preface_status =
				    analyze_http2_preface(request_line, &(reader->parser));

				if(http2_preface_status != Http2PrefaceStatusOk) {
					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = false,
						                   .value = { .enum_value =
						                                  HttpRequestErrorTypeInvalidHttp2Preface } } }
					};
				}

				return parse_http2_request(reader);
			}
			case ProtocolSelectedNone:
			default: {
				break;
			}
		}

		// check if the method makes sense

		if(request_line.method == HTTPRequestMethodPRI) {
			// this makes no sense here, if we use tls, the h2 alpn case would be checked earlier,
			// if we use no tls, the first request has to be http 1.1 and must negotiate an upgrade,
			// after the successfull upgrade, we can use http2
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = false,
				                   .value = { .enum_value =
				                                  HttpRequestErrorTypeInvalidHttp2Preface } } }
			};
		}

		// same as above (PRI method), http2 is not allwed in this path
		if(request_line.protocol_version == HTTPProtocolVersion2) {
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = false,
				                   .value = { .enum_value =
				                                  HttpRequestErrorTypeInvalidHttpVersion } } }
			};
		}

		{ // check path compatibilities
			if(request_line.uri.type == ParsedURITypeAsterisk) {
				if(request_line.method != HTTPRequestMethodOptions) {

					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = true,
						                   .value = { .advanced = "Invalid path in combination "
						                                          "with method: path '*' is only "
						                                          "supported with OPTIONS" } } }
					};

				} else {

					// TODO. implement further, search for todo_options
					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = true,
						                   .value = { .advanced =
						                                  "not implemented for method OPTIONS: "
						                                  "asterisk or normal request" } } }
					};
				}
			}

			if(request_line.uri.type == ParsedURITypeAuthority) {
				if(request_line.method != HTTPRequestMethodConnect) {

					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = true,
						                   .value = { .advanced = "Invalid path in combination "
						                                          "with method: path '*' is only "
						                                          "supported with OPTIONS" } } }
					};

				} else {

					// TODO. implement further, search for todo_options
					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = true,
						                   .value = { .advanced =
						                                  "not implemented for method CONNECT: "
						                                  "authority or normal request" } } }

					};
				}
			}
		}
	}

	return parse_http1_request(request_line, reader);
}

NODISCARD static HttpRequestResult parse_next_http_request(HTTPReader* const reader) {
	UNUSED(reader);
	UNREACHABLE();
}

HttpRequestResult get_http_request(HTTPReader* const reader) {

	if(!reader) {
		return (HttpRequestResult){ .is_error = true,
			                        .value = {
			                            .error = (HttpRequestError){
			                                .is_advanced = true,
			                                .value = { .advanced = "HTTP reader was null" } } } };
	}

	switch(reader->state) {
		case HTTPReaderStateEmpty: {
			SizedBuffer buffer = read_buffer_from_connection(reader->descriptor);

			if(buffer.data == NULL) {
				reader->state = HTTPReaderStateError;
				return (HttpRequestResult){
					.is_error = true,
					.value = { .error =
					               (HttpRequestError){
					                   .is_advanced = true,
					                   .value = { .advanced =
					                                  "HTTP reader state was invalid" } } }
				};
			}
			// TODO: if we fail to parse something, because not delimiter could be found, we should
			// just try to read more!
			reader->state = HTTPReaderStateReading;
			reader->parser.data = buffer;

			return parse_first_http_request(reader);
		}
		case HTTPReaderStateReading: {

			return parse_next_http_request(reader);
		}
		case HTTPReaderStateEnd:
		case HTTPReaderStateError:
		default: {
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = true,
				                   .value = { .advanced = "HTTP reader state was invalid" } } }
			};
		}
	}
}

NODISCARD bool http_reader_more_available(const HTTPReader* const reader) {
	switch(reader->state) {
		case HTTPReaderStateEmpty:
		case HTTPReaderStateReading: {
			return true;
		}
		case HTTPReaderStateEnd:
		case HTTPReaderStateError:
		default: {
			return false;
		}
	}
}

// TODO: the reader should tell us now if we have http 1.1 keepalive or http/2, but how does
// http/1.1 vs http/2 behave, do they close the connection, otherwise this can be false always?,
// investigate
#define SUPPORT_KEEPALIVE false

bool finish_reader(HTTPReader* reader, ConnectionContext* context) {

	// finally close the connection
	int result =
	    close_connection_descriptor_advanced(reader->descriptor, context, SUPPORT_KEEPALIVE);
	CHECK_FOR_ERROR(result, "While trying to close the connection descriptor", { return false; });

	free_sized_buffer(reader->data);
	free(reader);

	return true;
}
