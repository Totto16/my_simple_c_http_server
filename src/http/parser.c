#include "./parser.h"
#include "./header.h"
#include "./v2.h"
#include "generic/read.h"
#include "utils/buffered_reader.h"

#include <ctype.h>
#include <math.h>

NODISCARD HTTPRequestMethod get_http_method_from_string(const char* const method,
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
		return HTTPProtocolVersion1Dot0;
	}

	if(strcmp(protocol_version, "HTTP/1.1") == 0) {
		*success = true;
		return HTTPProtocolVersion1Dot1;
	}

	if(strcmp(protocol_version, "HTTP/2.0") == 0) {
		*success = true;
		return HTTPProtocolVersion2;
	}

	*success = false;
	return HTTPProtocolVersion1Dot0;
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HttpRequestLineResultTypeOk = 0,
	HttpRequestLineResultTypeError,
	HttpRequestLineResultTypeUnsupportedHttpVersion,
	HttpRequestLineResultTypeUnsupportedMethod,
	HttpRequestLineResultTypeUriError
} HttpRequestLineResultType;

typedef struct {
	HttpRequestLineResultType type;
	union {
		HttpRequestLine line;
	} data;
} HttpRequestLineResult;

// parse in string form, for http 1
NODISCARD static HttpRequestLineResult parse_http1_request_line(BufferedReader* const reader) {

	HttpRequestLine result = {};

	BufferedReadResult read_result =
	    buffered_reader_get_until_delimiter(reader, HTTP_LINE_SEPERATORS);

	if(read_result.type != BufferedReadResultTypeOk) {
		return (HttpRequestLineResult){ .type = HttpRequestLineResultTypeError };
	}

	// TODO(Totto): don't use libc parse function that operate on strings, use parser ones, that
	// operate on slices of bytes

	// make this string parseable by the libc functions
	SizedBuffer request_line = read_result.value.buffer;

	char* const start = (char*)request_line.data;
	*(start + request_line.size) = '\0';

	char* method = NULL;
	char* path = NULL;
	char* protocol_version = NULL;

	{ // parse the three filed in the line from start to line_end

		char* method_end = strchr(start, ' ');

		if(method_end == NULL) {
			return (HttpRequestLineResult){ .type = HttpRequestLineResultTypeError };
		}

		*method_end = '\0';

		method = start;

		path = method_end + 1;

		char* path_end = strchr(path, ' ');

		if(path_end == NULL) {
			return (HttpRequestLineResult){ .type = HttpRequestLineResultTypeError };
		}

		*path_end = '\0';

		protocol_version = path_end + 1;
	}

	ParsedRequestURIResult uri_result = parse_request_uri(path);

	if(uri_result.is_error) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		            "Invalid uri in HTTP request: %s\n", uri_result.value.error);
		return (HttpRequestLineResult){ .type = HttpRequestLineResultTypeUriError };
	}

	result.uri = uri_result.value.uri;

	bool success = false;

	result.method = get_http_method_from_string(method, &success);

	if(!success) {
		free_parsed_request_uri(result.uri);
		return (HttpRequestLineResult){ .type = HttpRequestLineResultTypeUnsupportedMethod };
	}

	result.protocol_version = get_protocol_version_from_string(protocol_version, &success);

	if(!success) {
		free_parsed_request_uri(result.uri);
		return (HttpRequestLineResult){ .type = HttpRequestLineResultTypeUnsupportedHttpVersion };
	}

	return (HttpRequestLineResult){
		.type = HttpRequestLineResultTypeOk,
		.data = { .line = result },
	};
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
	HTTPContextTypeV1 = 0,
	HTTPContextTypeV1Keepalive,
	HTTPContextTypeV2,
} HTTPContextType;

struct HTTPGeneralContextImpl {
	HTTPContextType type;
	union {
		HTTP2Context v2;
	} data;
};

struct HTTPReaderImpl {
	ProtocolSelected protocol;
	//
	BufferedReader* buffered_reader;
	HTTPReaderState state;
	HTTPGeneralContext general_context;
};

NODISCARD HTTPReader* NULLABLE
initialize_http_reader_from_connection(ConnectionDescriptor* const descriptor) {

	HTTPReader* reader = malloc(sizeof(HTTPReader));

	if(!reader) {
		return NULL;
	}

	BufferedReader* buffered_reader = get_buffered_reader(descriptor);

	if(!buffered_reader) {
		free(reader);
		return NULL;
	}

	ProtocolSelected protocol = get_selected_protocol(descriptor);

	*reader = (HTTPReader){
		.protocol = protocol,
		.state = HTTPReaderStateEmpty,
		.buffered_reader = buffered_reader,
		.general_context = (HTTPGeneralContext){ .type = HTTPContextTypeV1 },
	};

	return reader;
}

static CompressionType parse_compression_type(char* compression_name, OUT_PARAM(bool) ok_result) {
	// see: https://datatracker.ietf.org/doc/html/rfc7230#section-4.2.3
	if(strcmp(compression_name, "gzip") == 0 || strcmp(compression_name, "x-gzip") == 0) {
		*ok_result = true;
		return CompressionTypeGzip;
	}

	// see: https://datatracker.ietf.org/doc/html/rfc7230#section-4.2.2
	if(strcmp(compression_name, "deflate") == 0) {
		*ok_result = true;
		return CompressionTypeDeflate;
	}

	if(strcmp(compression_name, "br") == 0) {
		*ok_result = true;
		return CompressionTypeBr;
	}

	if(strcmp(compression_name, "zstd") == 0) {
		*ok_result = true;
		return CompressionTypeZstd;
	}

	// see: https://datatracker.ietf.org/doc/html/rfc7230#section-4.2.1
	if(strcmp(compression_name, "compress") == 0 || strcmp(compression_name, "x-compress") == 0) {
		*ok_result = true;
		return CompressionTypeCompress;
	}

	LOG_MESSAGE(LogLevelWarn, "Not recognized compression level: %s\n", compression_name);

	*ok_result = false;
	return CompressionTypeNone;
}

static CompressionValue parse_compression_value(char* compression_name, OUT_PARAM(bool) ok_result) {

	if(strcmp(compression_name, "*") == 0) {
		*ok_result = true;
		return (CompressionValue){ .type = CompressionValueTypeAllEncodings, .data = {} };
	}

	if(strcmp(compression_name, "identity") == 0) {
		*ok_result = true;
		return (CompressionValue){ .type = CompressionValueTypeNoEncoding, .data = {} };
	}

	CompressionValue result = { .type = CompressionValueTypeNormalEncoding, .data = {} };

	CompressionType type = parse_compression_type(compression_name, ok_result);

	if(!(*ok_result)) {
		return result;
	}

	result.data.normal_compression = type;
	*ok_result = true;

	return result;
}

NODISCARD static float parse_compression_quality(char* compression_weight) {
	// strip whitespace
	while(isspace(*compression_weight)) {
		compression_weight++;
	}

	if(strlen(compression_weight) < 2) {
		// no q=
		return NAN;
	}

	if(compression_weight[0] != 'q' && compression_weight[0] != 'Q') {
		return NAN;
	}
	compression_weight++;

	if(compression_weight[0] != '=') {
		return NAN;
	}
	compression_weight++;

	float value = parse_float(compression_weight);

	return value;
}

NODISCARD static HttpRequestProperties get_http_properties(const HttpRequest http_request) {

	HttpRequestProperties http_properties = { .type = HTTPPropertyTypeInvalid };

	ParsedRequestURI uri = http_request.head.request_line.uri;

	const ParsedURLPath* path = NULL;

	switch(uri.type) {
		case ParsedURITypeAbsoluteURI: {
			path = &uri.data.uri.path;
			break;
		}
		case ParsedURITypeAbsPath: {
			path = &uri.data.path;
			break;
		}
		case ParsedURITypeAsterisk:
		case ParsedURITypeAuthority:
		default: {
			http_properties.type = HTTPPropertyTypeInvalid;

			return http_properties;
		}
	}

	if(path == NULL) {
		http_properties.type = HTTPPropertyTypeInvalid;

		return http_properties;
	}

	switch(http_request.head.request_line.method) {
		case HTTPRequestMethodGet:
		case HTTPRequestMethodPost:
		case HTTPRequestMethodHead: {

			http_properties.type = HTTPPropertyTypeNormal;
			http_properties.data.normal = *path;

			return http_properties;
		}
		case HTTPRequestMethodOptions: {
			http_properties.type = HTTPPropertyTypeOptions;
			http_properties.data.todo_options = 1;

			return http_properties;
		}
		case HTTPRequestMethodConnect: {
			http_properties.type = HTTPPropertyTypeConnect;
			http_properties.data.todo_connect = 1;

			return http_properties;
		}
		default: {
			http_properties.type = HTTPPropertyTypeInvalid;

			return http_properties;
		}
	}
}

NODISCARD CompressionSettings get_compression_settings(HttpHeaderFields header_fields) {

	CompressionSettings compression_settings = { .entries = TVEC_EMPTY(CompressionEntry) };

	// see: https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

	HttpHeaderField* accept_encoding_header =
	    find_header_by_key(header_fields, HTTP_HEADER_NAME(accept_encoding));

	if(!accept_encoding_header) {
		return compression_settings;
	}

	char* raw_value = accept_encoding_header->value;

	if(strlen(raw_value) == 0) {
		return compression_settings;
	}

	// copy the value, so that parsing is easier

	char* value = strdup(raw_value);
	char* original_value = value;

	do {

		char* index = strstr(value, ",");

		if(index != NULL) {
			*index = '\0';
		}

		// value points to the string to parse, that is null terminated

		{

			char* sub_index = strstr(value, ";");

			char* compression_name = value;
			char* compression_weight = NULL;

			if(sub_index != NULL) {
				*sub_index = '\0';
				compression_weight = sub_index + 1;
			}

			CompressionEntry entry = { .value = {}, .weight = 1.0F };

			if(compression_weight != NULL) {

				float weight_value = parse_compression_quality(compression_weight);

				if(!isnan(weight_value)) {
					entry.weight = weight_value;
				}
			}

			// strip whitespace
			while(isspace(*compression_name)) {
				compression_name++;
			}

			bool ok_result = true;
			CompressionValue comp_value = parse_compression_value(compression_name, &ok_result);

			if(ok_result) {
				entry.value = comp_value;

				auto _ = TVEC_PUSH(CompressionEntry, &(compression_settings.entries), entry);
				UNUSED(_);
			} else {
				LOG_MESSAGE(LogLevelWarn, "Couldn't parse compression '%s'\n", compression_name);
			}
		}

		if(index == NULL) {
			break;
		}

		value = index + 1;

		{

			// strip whitespace
			while(isspace(*value)) {
				value++;
			}
		}

	} while(true);

	free(original_value);
	return compression_settings;
}

void free_compression_settings(CompressionSettings compression_settings) {
	TVEC_FREE(CompressionEntry, &(compression_settings.entries));
}

void free_request_settings(RequestSettings request_settings) {

	free_compression_settings(request_settings.compression_settings);
}

NODISCARD RequestSettings get_request_settings(const HttpRequest http_request) {

	RequestSettings request_settings = {
		.compression_settings =
		    (CompressionSettings){
		        .entries = TVEC_EMPTY(CompressionEntry),
		    },
		.protocol_used = http_request.head.request_line.protocol_version,
		.http_properties = { .type = HTTPPropertyTypeInvalid },
	};

	CompressionSettings compression_settings =
	    get_compression_settings(http_request.head.header_fields);

	request_settings.compression_settings = compression_settings;

	request_settings.http_properties = get_http_properties(http_request);

	// TODO(Totto): body encoding

	return request_settings;
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRequestLengthTypeNoBody = 0,
	HTTPRequestLengthTypeClose,
	HTTPRequestLengthTypeContentLength,
	HTTPRequestLengthTypeTransferEncoded,
} HTTPRequestLengthType;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPEncodingChunked = 0,
	// TODO(Totto): also support compressions with chunked
} HTTPEncoding;

typedef struct {
	HTTPRequestLengthType type;
	union {
		size_t length;
		HTTPEncoding encoding;
	} value;
} HTTPRequestLength;

typedef struct {
	HTTPRequestLength length;
	RequestSettings settings;
} HTTPAnalyzeHeaders;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPAnalyzeHeaderErrorProtocolError = 0,
	HTTPAnalyzeHeaderErrorNotSupported,
} HTTPAnalyzeHeaderError;

typedef struct {
	bool is_error;
	union {
		HTTPAnalyzeHeaderError error;
		HTTPAnalyzeHeaders result;
	} data;
} HTTPAnalyzeHeadersResult;

NODISCARD static HTTPAnalyzeHeadersResult http_analyze_headers(const HttpRequest http_request) {

	HTTPAnalyzeHeaders analyze_result = {
		.length = { .type = HTTPRequestLengthTypeNoBody },
	};

	// the body length is determined as given by this rfc entry:
	// https://datatracker.ietf.org/doc/html/rfc9112#section-6.3

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, http_request.head.header_fields); ++i) {
		HttpHeaderField header = TVEC_AT(HttpHeaderField, http_request.head.header_fields, i);

		if(strcasecmp(header.key, HTTP_HEADER_NAME(content_length)) == 0) {
			if(analyze_result.length.type != HTTPRequestLengthTypeNoBody) {
				// both transfer-encoding and length are used

				return (HTTPAnalyzeHeadersResult){
					.is_error = true,
					.data = { .error = HTTPAnalyzeHeaderErrorProtocolError },
				};
			}

			bool success = false;

			const size_t content_length = parse_size_t(header.value, &success);

			if(!success) {
				return (HTTPAnalyzeHeadersResult){
					.is_error = true,
					.data = { .error = HTTPAnalyzeHeaderErrorProtocolError },
				};
			}

			analyze_result.length.type = HTTPRequestLengthTypeContentLength;
			analyze_result.length.value.length = content_length;

		} else if(strcasecmp(header.key, HTTP_HEADER_NAME(transfer_encoding)) == 0) {

			if(analyze_result.length.type != HTTPRequestLengthTypeNoBody) {
				// both transfer-encoding and length are used

				return (HTTPAnalyzeHeadersResult){
					.is_error = true,
					.data = { .error = HTTPAnalyzeHeaderErrorProtocolError },
				};
			}

			analyze_result.length.type = HTTPRequestLengthTypeTransferEncoded;

			// TODO(Totto): support more than chunked, as also compression can be used with chunked!
			if(strcasecmp(header.value, "chunked") != 0) {
				return (HTTPAnalyzeHeadersResult){
					.is_error = true,
					.data = { .error = HTTPAnalyzeHeaderErrorNotSupported },
				};
			}
			analyze_result.length.value.encoding = HTTPEncodingChunked;

		} else if(strcasecmp(header.key, HTTP_HEADER_NAME(connection)) == 0) {
			// see https://datatracker.ietf.org/doc/html/rfc7230#section-6.1
			if(strcasecmp(header.value, "close") == 0) {
				analyze_result.length.type = HTTPRequestLengthTypeClose;
			}

			// TODO(Totto): handle keepalive

		} else if(strcasecmp(header.key, HTTP_HEADER_NAME(connection)) == 0) {
			// see: https://datatracker.ietf.org/doc/html/rfc7230#section-6.7

			// char* value = strdup(header.value);

			// TODO(Totto): implement
			LOG_MESSAGE_SIMPLE(LogLevelWarn, "TODO: implement h2 upgrade check\n");
		}
	}

	// only allow HTTPRequestLengthTypeClose on http/1.0
	if(analyze_result.length.type == HTTPRequestLengthTypeClose &&
	   http_request.head.request_line.protocol_version != HTTPProtocolVersion1Dot0) {
		analyze_result.length.type = HTTPRequestLengthTypeNoBody;
	}

	analyze_result.settings = get_request_settings(http_request);

	return (HTTPAnalyzeHeadersResult){
		.is_error = false,
		.data = { .result = analyze_result },
	};
}

typedef struct {
	bool is_error;
	union {
		const char* error;
		SizedBuffer body;
	} data;
} HttpBodyReadResult;

NODISCARD static HttpBodyReadResult get_http_body(HTTPReader* const reader,
                                                  const HTTPAnalyzeHeaders analyze) {

	switch(analyze.length.type) {
		case HTTPRequestLengthTypeClose: {

			const BufferedReadResult res = buffered_reader_get_until_end(reader->buffered_reader);

			if(res.type != BufferedReadResultTypeOk) {
				return (HttpBodyReadResult){
					.is_error = true,
					.data = { .error = "read failed" },
				};
			}

			reader->state = HTTPReaderStateEnd;

			return (HttpBodyReadResult){
				.is_error = false,
				.data = { .body = sized_buffer_dup(res.value.buffer) },
			};
		}

		case HTTPRequestLengthTypeContentLength: {

			const BufferedReadResult res =
			    buffered_reader_get_amount(reader->buffered_reader, analyze.length.value.length);

			if(res.type != BufferedReadResultTypeOk) {
				return (HttpBodyReadResult){
					.is_error = true,
					.data = { .error = "read failed" },
				};
			}

			return (HttpBodyReadResult){
				.is_error = false,
				.data = { .body = res.value.buffer },
			};
		}
		case HTTPRequestLengthTypeTransferEncoded: {
			// TODO(Totto): implement
			return (HttpBodyReadResult){
				.is_error = true,
				.data = { .error = "transfer encoding not yet implemented" },
			};
		}
		case HTTPRequestLengthTypeNoBody: {
			return (HttpBodyReadResult){
				.is_error = false,
				.data = { .body = (SizedBuffer){ .data = NULL, .size = 0 } },
			};
		}
		default: {
			return (HttpBodyReadResult){
				.is_error = true,
				.data = { .error = "invalid length type" },
			};
		}
	}
}

NODISCARD static HttpRequestResult parse_http1_request(const HttpRequestLine request_line,
                                                       HTTPReader* const reader) {

	HttpRequest request = {
		.head =
		    (HttpRequestHead){
		        .request_line = request_line,
		        .header_fields = TVEC_EMPTY(HttpHeaderField),
		    },
		.body = (SizedBuffer){ .data = NULL, .size = 0 },
	};

	// TODO(Totto): don't use libc parse function that operate on strings, use parser ones, that
	// operate on slices of bytes

	// parse headers
	while(true) {

		BufferedReadResult read_result =
		    buffered_reader_get_until_delimiter(reader->buffered_reader, HTTP_LINE_SEPERATORS);

		if(read_result.type != BufferedReadResultTypeOk) {
			return (
			    HttpRequestResult){ .is_error = true,
				                    .value = { .error = (HttpRequestError){
				                                   .is_advanced = true,
				                                   .value = { .advanced =
				                                                  "Failed to parse headers" } } } };
		}

		SizedBuffer header_line = read_result.value.buffer;

		char* const current_pos = (char*)header_line.data;
		*(current_pos + header_line.size) = '\0';

		if(strlen(current_pos) == 0) {
			break;
		}

		char* header_start = current_pos;

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

			auto _ = TVEC_PUSH(HttpHeaderField, &(request.head.header_fields), field);
			UNUSED(_);
		}
	}

	const HTTPAnalyzeHeadersResult analyze_result = http_analyze_headers(request);

	if(analyze_result.is_error) {

		HttpRequestErrorType enum_value = HttpRequestErrorTypeProtocolError;
		switch(analyze_result.data.error) {
			case HTTPAnalyzeHeaderErrorProtocolError: {
				enum_value = HttpRequestErrorTypeProtocolError;
				break;
			}
			case HTTPAnalyzeHeaderErrorNotSupported: {
				enum_value = HttpRequestErrorTypeNotSupported;
				break;
			}
			default: {
				enum_value = HttpRequestErrorTypeProtocolError;
				break;
			}
		}

		return (HttpRequestResult){ .is_error = true,
			                        .value = { .error = (HttpRequestError){
			                                       .is_advanced = false,
			                                       .value = { .enum_value = enum_value } } } };
	}

	const HTTPAnalyzeHeaders analyze = analyze_result.data.result;

	const HttpBodyReadResult body_result = get_http_body(reader, analyze);

	if(body_result.is_error) {
		return (HttpRequestResult){
			.is_error = true,
			.value = { .error =
			               (HttpRequestError){ .is_advanced = true,
			                                   .value = { .advanced = body_result.data.error } } }
		};
	}

	request.body = body_result.data.body;

	// check if the request body makes sense
	if((request_line.method == HTTPRequestMethodGet ||
	    request_line.method == HTTPRequestMethodHead ||
	    request_line.method == HTTPRequestMethodOptions) &&
	   request.body.size != 0) {

		return (HttpRequestResult){
			.is_error = true,
			.value = { .error =
			               (HttpRequestError){
			                   .is_advanced = false,
			                   .value = { .enum_value =
			                                  HttpRequestErrorTypeInvalidNonEmptyBody } } }
		};
	}

	return (HttpRequestResult){ .is_error = false,
		                        .value = { .result = {
		                                       .request = request,
		                                       .settings = analyze.settings,
		                                   } } };
}

NODISCARD static HttpRequestResult parse_first_http_request(HTTPReader* const reader) {

	// only possible in the first request, as also http2 must send a valid http1 request line
	// (the preface)
	HttpRequestLineResult request_line_result = parse_http1_request_line(reader->buffered_reader);

	switch(request_line_result.type) {
		case HttpRequestLineResultTypeOk: {
			break;
		}

		case HttpRequestLineResultTypeUnsupportedHttpVersion: {
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = false,
				                   .value = { .enum_value =
				                                  HttpRequestErrorTypeInvalidHttpVersion } } }
			};
		}
		case HttpRequestLineResultTypeUnsupportedMethod: {
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = false,
				                   .value = { .enum_value =
				                                  HttpRequestErrorTypeMethodNotSupported } } }
			};
		}
		case HttpRequestLineResultTypeUriError: {
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = true,
				                   .value = { .advanced =
				                                  "failed to parse URi in request line" } } }
			};
		}
		case HttpRequestLineResultTypeError:
		default: {
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = true,
				                   .value = { .advanced = "failed to parse request line" } } }
			};
		}
	}

	HttpRequestLine request_line = request_line_result.data.line;

	{ // check for logic errors, this differs in the first request and the following ones,
	  // as the first request might negotiate some things

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

				// this consumes more bytes, if the request line matches, otherwise it is an
				// error if it matches the data after the reader cursors position is already
				// the http2 request!
				const Http2PrefaceStatus http2_preface_status =
				    analyze_http2_preface(request_line, reader->buffered_reader);

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

				reader->general_context.type = HTTPContextTypeV2;
				reader->general_context.data.v2 = http2_default_context();

				const Http2StartResult start_result = http2_send_and_receive_preface(
				    &reader->general_context.data.v2, reader->buffered_reader);

				if(start_result.is_error) {
					return (HttpRequestResult){
						.is_error = true,
						.value = { .error =
						               (HttpRequestError){
						                   .is_advanced = false,
						                   .value = { .enum_value =
						                                  HttpRequestErrorTypeInvalidHttp2Preface } } }
					};
				}

				return parse_http2_request(&(reader->general_context.data.v2),
				                           reader->buffered_reader);
			}
			case ProtocolSelectedNone:
			default: {
				break;
			}
		}

		// check if the method makes sense

		if(request_line.protocol_version == HTTPProtocolVersion2) {
			// the first request was already a http2 request

			const Http2PrefaceStatus http2_preface_status =
			    analyze_http2_preface(request_line, reader->buffered_reader);

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

			reader->general_context.type = HTTPContextTypeV2;
			reader->general_context.data.v2 = http2_default_context();

			const Http2StartResult start_result = http2_send_and_receive_preface(
			    &reader->general_context.data.v2, reader->buffered_reader);

			if(start_result.is_error) {
				return (HttpRequestResult){
					.is_error = true,
					.value = { .error =
					               (HttpRequestError){
					                   .is_advanced = true,
					                   .value = { .advanced = start_result.value.error } } }
				};
			}

			return parse_http2_request(&(reader->general_context.data.v2), reader->buffered_reader);
		}

		if(request_line.method == HTTPRequestMethodPRI) {
			// this makes no sense here, as this can be only used with an http2 preface
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = false,
				                   .value = { .enum_value =
				                                  HttpRequestErrorTypeInvalidHttp2Preface } } }
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
				}

				// TODO(Totto): implement further, search for todo_options
				return (
				    HttpRequestResult){ .is_error = true,
					                    .value = { .error = (HttpRequestError){
					                                   .is_advanced = true,
					                                   .value = { .advanced = "not implemented for "
					                                                          "method OPTIONS: "
					                                                          "asterisk or normal "
					                                                          "request" } } } };
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
				}

				// TODO(Totto): implement further, search for todo_options
				return (HttpRequestResult){
					.is_error = true,
					.value = { .error =
					               (HttpRequestError){ .is_advanced = true,
					                                   .value = { .advanced = "not implemented for "
					                                                          "method CONNECT: "
					                                                          "authority or normal "
					                                                          "request" } } }

				};
			}
		}
	}

	const HttpRequestResult result = parse_http1_request(request_line, reader);

	if(result.is_error) {
		reader->state = HTTPReaderStateError;
	}

	if(reader->general_context.type == HTTPContextTypeV1) {

		// TODO(Totto): use eof, but that has problems, as some clients only close, after we have
		// closed
		//  try to figure out, what the best solution would be
		// see also finish_buffered_reader()

		if(buffered_reader_has_more_data(reader->buffered_reader)) {
			reader->state = HTTPReaderStateEnd;
		} else {
			// if we are not in a keepalive situation, any presence of a body is an error
			reader->state = HTTPReaderStateError;
			return (HttpRequestResult){
				.is_error = true,
				.value = { .error =
				               (HttpRequestError){
				                   .is_advanced = false,
				                   .value = { .enum_value = HttpRequestErrorTypeLengthRequired } } }
			};
		}
	}

	return result;
}

NODISCARD static HttpRequestResult parse_next_http_request(HTTPReader* const reader) {

	if(reader->general_context.type == HTTPContextTypeV2) {
		return parse_http2_request(&(reader->general_context.data.v2), reader->buffered_reader);
	}

	return (HttpRequestResult){ .is_error = true,
		                        .value = { .error = (HttpRequestError){
		                                       .is_advanced = true,
		                                       .value = { .advanced = "Not yet supported" } } } };
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

			reader->state = HTTPReaderStateReading;

			return parse_first_http_request(reader);
		}
		case HTTPReaderStateReading: {
			if(reader->general_context.type == HTTPContextTypeV1) {
				return (HttpRequestResult){
					.is_error = true,
					.value = { .error =
					               (HttpRequestError){
					                   .is_advanced = true,
					                   .value = { .advanced = "HTTP reader tried to read more than "
					                                          "one request on a non compatible "
					                                          "connection" } } }
				};
			}

			// this clear the buffer until the current cursor, everything we references from
			// that is long dead now (hopefully xD, http2 or http1 keepalive state should not
			// hold onto strings from there)
			buffered_reader_invalidate_old_data(reader->buffered_reader);

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

static void free_reader_general_context(HTTPGeneralContext general_context) {

	switch(general_context.type) {
		case HTTPContextTypeV1:
		case HTTPContextTypeV1Keepalive: {
			break;
		}
		case HTTPContextTypeV2: {
			free_http2_context(general_context.data.v2);
			break;
		}
		default: {
			break;
		}
	}
}

// TODO(Totto): the reader should tell us now if we have http 1.1 keepalive or http/2, but how does
// http/1.1 vs http/2 behave, do they close the connection, otherwise this can be false always?,
// investigate
#define SUPPORT_KEEPALIVE false

bool finish_reader(HTTPReader* reader, ConnectionContext* context) {

	// finally close the connection

	if(!finish_buffered_reader(reader->buffered_reader, context, SUPPORT_KEEPALIVE)) {
		return false;
	}

	free_reader_general_context(reader->general_context);
	free(reader);

	return true;
}

NODISCARD HTTPGeneralContext* http_reader_get_general_context(HTTPReader* const reader) {
	return &(reader->general_context);
}

NODISCARD BufferedReader* http_reader_release_buffered_reader(HTTPReader* const reader) {
	BufferedReader* buffered_reader = reader->buffered_reader;

	reader->buffered_reader = NULL;

	return buffered_reader;
}

NODISCARD HTTP2Context*
http_general_context_get_http2_context(HTTPGeneralContext* const general_context) {
	if(general_context->type != HTTPContextTypeV2) {
		return NULL;
	}

	return &(general_context->data.v2);
}
