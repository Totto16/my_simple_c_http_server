
#include "./protocol.h"
#include "./parser.h"

TVEC_IMPLEMENT_VEC_TYPE(HttpHeaderField)

TVEC_IMPLEMENT_VEC_TYPE(CompressionEntry)

NODISCARD const char* get_http_method_string(HTTPRequestMethod method) {
	switch(method) {
		case HTTPRequestMethodGet: return "GET";
		case HTTPRequestMethodPost: return "POST";
		case HTTPRequestMethodHead: return "HEAD";
		case HTTPRequestMethodOptions: return "OPTIONS";
		case HTTPRequestMethodConnect: return "CONNECT";
		case HTTPRequestMethodPRI: return "PRI";
		default: return "<Unknown>";
	}
}

NODISCARD const char* get_http_protocol_version_string(HTTPProtocolVersion protocol_version) {

	switch(protocol_version) {
		case HTTPProtocolVersion1Dot0: return "HTTP/1.0";
		case HTTPProtocolVersion1Dot1: return "HTTP/1.1";
		case HTTPProtocolVersion2: return "HTTP/2.0";
		default: return "<Unknown>";
	}
}

static void free_http_request_line(HttpRequestLine line) {
	free_parsed_request_uri(line.uri);
}

static void free_request_head(HttpRequestHead head) {
	free_http_request_line(head.request_line);
	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, head.header_fields); ++i) {
		HttpHeaderField entry = TVEC_AT(HttpHeaderField, head.header_fields, i);

		free(entry.key);
		free(entry.value);
	}
	TVEC_FREE(HttpHeaderField, &head.header_fields);
}

// frees the HttpRequest, taking care of Null Pointer, this si needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void free_http_request(HttpRequest request) {
	free_request_head(request.head);
	free_sized_buffer(request.body);
}

void free_http_request_result(HTTPResultOk result) {
	free_http_request(result.request);
	free_request_settings(result.settings);
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

NODISCARD const ParsedSearchPathEntry* find_search_key(ParsedSearchPath search_path,
                                                       const char* key) {

	if(TMAP_IS_EMPTY(ParsedSearchPathHashMap, &search_path.hash_map)) {
		return NULL;
	}

	const ParsedSearchPathEntry* entry =
	    TMAP_GET_ENTRY(ParsedSearchPathHashMap, &(search_path.hash_map), key);

	if(entry == NULL) {
		return NULL;
	}

	return entry;
}

// simple helper for getting the status Message for a special status code, all from the spec for
// http 1.1 implemented (not in the spec e.g. 418)
const char* get_status_message(HttpStatusCode status_code) {
	const char* result = "NOT SUPPORTED STATUS CODE"; // NOLINT(clang-analyzer-deadcode.DeadStores)
	// according to https://datatracker.ietf.org/doc/html/rfc7231#section-6.1
	switch(status_code) {
		case HttpStatusContinue: result = "Continue"; break;
		case HttpStatusSwitchingProtocols: result = "Switching Protocols"; break;
		case HttpStatusOk: result = "OK"; break;
		case HttpStatusCreated: result = "Created"; break;
		case HttpStatusAccepted: result = "Accepted"; break;
		case HttpStatusNonAuthoritativeInformation: result = "Non-Authoritative Information"; break;
		case HttpStatusNoContent: result = "No Content"; break;
		case HttpStatusResetContent: result = "Reset Content"; break;
		case HttpStatusPartialContent: result = "Partial Content"; break;
		case HttpStatusMultipleChoices: result = "Multiple Choices"; break;
		case HttpStatusMovedPermanently: result = "Moved Permanently"; break;
		case HttpStatusFound: result = "Found"; break;
		case HttpStatusSeeOther: result = "See Other"; break;
		case HttpStatusNotModified: result = "Not Modified"; break;
		case HttpStatusUseProxy: result = "Use Proxy"; break;
		case HttpStatusTemporaryRedirect: result = "Temporary Redirect"; break;
		case HttpStatusBadRequest: result = "Bad Request"; break;
		case HttpStatusUnauthorized: result = "Unauthorized"; break;
		case HttpStatusPaymentRequired: result = "Payment Required"; break;
		case HttpStatusForbidden: result = "Forbidden"; break;
		case HttpStatusNotFound: result = "Not Found"; break;
		case HttpStatusMethodNotAllowed: result = "Method Not Allowed"; break;
		case HttpStatusNotAcceptable: result = "Not Acceptable"; break;
		case HttpStatusProxyAuthenticationRequired: result = "Proxy Authentication Required"; break;
		case HttpStatusRequestTimeout: result = "Request Timeout"; break;
		case HttpStatusConflict: result = "Conflict"; break;
		case HttpStatusGone: result = "Gone"; break;
		case HttpStatusLengthRequired: result = "Length Required"; break;
		case HttpStatusPreconditionFailed: result = "Precondition Failed"; break;
		case HttpStatusPayloadTooLarge: result = "Payload Too Large"; break;
		case HttpStatusUriTooLong: result = "URI Too Long"; break;
		case HttpStatusUnsupportedMediaType: result = "Unsupported Media Type"; break;
		case HttpStatusRangeNotSatisfiable: result = "Range Not Satisfiable"; break;
		case HttpStatusExpectationFailed: result = "Expectation Failed"; break;
		case HttpStatusUpgradeRequired: result = "Upgrade Required"; break;
		case HttpStatusInternalServerError: result = "Internal Server Error"; break;
		case HttpStatusNotImplemented: result = "Not Implemented"; break;
		case HttpStatusBadGateway: result = "Bad Gateway"; break;
		case HttpStatusServiceUnavailable: result = "Service Unavailable"; break;
		case HttpStatusGatewayTimeout: result = "Gateway Timeout"; break;
		case HttpStatusHttpVersionNotSupported: result = "HTTP Version Not Supported"; break;
		default: break;
	}
	return result;
}

NODISCARD HttpHeaderField* find_header_by_key(HttpHeaderFields array, const char* key) {

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, array); ++i) {
		HttpHeaderField* header = TVEC_GET_AT_MUT(HttpHeaderField, &array, i);
		if(strcasecmp(header->key, key) == 0) {
			return header;
		}
	}

	return NULL;
}

#define COMPRESSIONS_SIZE 5

static CompressionType get_best_compression_that_is_supported(void) {

	// This are sorted by compression ratio, not by speed , but this may be inaccurate
	CompressionType supported_compressions[COMPRESSIONS_SIZE] = {
		CompressionTypeBr,      CompressionTypeZstd,     CompressionTypeGzip,
		CompressionTypeDeflate, CompressionTypeCompress,
	};

	for(size_t i = 0; i < COMPRESSIONS_SIZE; ++i) {
		CompressionType compression = supported_compressions[i];
		if(is_compression_supported(compression)) {
			return compression;
		}
	}

	return CompressionTypeNone;
}

static int
compare_function_entries(const CompressionEntry* // NOLINT(bugprone-easily-swappable-parameters)
                             entry1,
                         const CompressionEntry* entry2) {

	// note weight is between 0.0 and 1.0

	if(entry1->weight != entry2->weight) {
		return (
		    int)((entry1->weight - entry2->weight) *
		         10000.0F); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	return 0;
}

SendSettings get_send_settings(const RequestSettings request_settings) {

	SendSettings result = {
		.compression_to_use = CompressionTypeNone,
		.protocol_to_use = request_settings.protocol_used,
	};

	CompressionEntries entries = request_settings.compression_settings.entries;

	size_t entries_length = TVEC_LENGTH(CompressionEntry, entries);

	if(entries_length == 0) {
		return result;
	}

	// this sorts the entries by weight, same weight means, we prefer the ones that come first
	// in the string, as it is unspecified in the spec, on what to sort as 2. criterium
	TVEC_SORT(CompressionEntry, &entries, compare_function_entries);

	for(size_t i = 0; i < entries_length; ++i) {
		CompressionEntry entry = TVEC_AT(CompressionEntry, entries, i);

		switch(entry.value.type) {
			case CompressionValueTypeNoEncoding: {
				result.compression_to_use = CompressionTypeNone;
				goto break_for;
			}
			case CompressionValueTypeAllEncodings: {
				result.compression_to_use = get_best_compression_that_is_supported();
				goto break_for;
			}
			case CompressionValueTypeNormalEncoding: {
				if(is_compression_supported(entry.value.data.normal_compression)) {
					result.compression_to_use = entry.value.data.normal_compression;
					goto break_for;
				}
				break;
			}
			default: {
				result.compression_to_use = CompressionTypeNone;
				goto break_for;
			}
		}
	}
break_for:

	return result;
}

void free_http_header_field(const HttpHeaderField field) {
	free(field.key);
	free(field.value);
}

void free_http_header_fields(HttpHeaderFields* header_fields) {
	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, *header_fields); ++i) {
		// same elegant freeing but two at once :)

		HttpHeaderField field = TVEC_AT(HttpHeaderField, *header_fields, i);

		free_http_header_field(field);
	}
	TVEC_FREE(HttpHeaderField, header_fields);
	*header_fields = TVEC_EMPTY(HttpHeaderField);
}

static void add_http_header_field_raw(HttpHeaderFields* const header_fields, char* const key,
                                      char* const value) {

	HttpHeaderField field = { .key = key, .value = value };

	auto _ = TVEC_PUSH(HttpHeaderField, header_fields, field);
	UNUSED(_);
}

void add_http_header_field_const_key_dynamic_value(HttpHeaderFields* const header_fields,
                                                 const char* const key, char* const value) {

	add_http_header_field_raw(header_fields, strdup(key), value);
}

void add_http_header_field_const_key_const_value(HttpHeaderFields* const header_fields,
                                                 const char* const key, const char* const value) {
	add_http_header_field_raw(header_fields, strdup(key), strdup(value));
}
