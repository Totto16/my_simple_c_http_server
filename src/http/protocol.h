

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// needed h files
#include "./compression.h"

#include "./uri.h"
#include "generic/secure.h"
#include "utils/log.h"
#include "utils/sized_buffer.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

#include <tvec.h>

// according to https://datatracker.ietf.org/doc/html/rfc7231#section-6.1
// + 418
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	HttpStatusContinue = 100,
	HttpStatusSwitchingProtocols = 101,
	//
	HttpStatusOk = 200,
	HttpStatusCreated = 201,
	HttpStatusAccepted = 202,
	HttpStatusNonAuthoritativeInformation = 203,
	HttpStatusNoContent = 204,
	HttpStatusResetContent = 205,
	HttpStatusPartialContent = 206,
	//
	HttpStatusMultipleChoices = 300,
	HttpStatusMovedPermanently = 301,
	HttpStatusFound = 302,
	HttpStatusSeeOther = 303,
	HttpStatusNotModified = 304,
	HttpStatusUseProxy = 305,
	HttpStatusTemporaryRedirect = 307,
	//
	HttpStatusBadRequest = 400,
	HttpStatusUnauthorized = 401,
	HttpStatusPaymentRequired = 402,
	HttpStatusForbidden = 403,
	HttpStatusNotFound = 404,
	HttpStatusMethodNotAllowed = 405,
	HttpStatusNotAcceptable = 406,
	HttpStatusProxyAuthenticationRequired = 407,
	HttpStatusRequestTimeout = 408,
	HttpStatusConflict = 409,
	HttpStatusGone = 410,
	HttpStatusLengthRequired = 411,
	HttpStatusPreconditionFailed = 412,
	HttpStatusPayloadTooLarge = 413,
	HttpStatusUriTooLong = 414,
	HttpStatusUnsupportedMediaType = 415,
	HttpStatusRangeNotSatisfiable = 416,
	HttpStatusExpectationFailed = 417,
	HttpStatusUpgradeRequired = 426,
	//
	HttpStatusInternalServerError = 500,
	HttpStatusNotImplemented = 501,
	HttpStatusBadGateway = 502,
	HttpStatusServiceUnavailable = 503,
	HttpStatusGatewayTimeout = 504,
	HttpStatusHttpVersionNotSupported = 505,
} HttpStatusCode;

// self implemented Http Request and Http Response Structs

typedef struct {
	tstr key;
	tstr value;
} HttpHeaderField;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRequestMethodGet = 0,
	HTTPRequestMethodPost,
	HTTPRequestMethodHead,
	HTTPRequestMethodOptions,
	HTTPRequestMethodConnect,
	HTTPRequestMethodPRI
} HTTPRequestMethod;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPProtocolVersion1Dot0,
	HTTPProtocolVersion1Dot1,
	HTTPProtocolVersion2,
} HTTPProtocolVersion;

#define DEFAULT_RESPONSE_PROTOCOL_VERSION HTTPProtocolVersion1Dot1

typedef struct {
	bool _reserved : 1;
	uint32_t identifier : 31; // only 31 bits!
} Http2Identifier;

typedef struct {
	Http2Identifier stream_identifier;
} HttpProtocolDataV2;

typedef struct {
	HTTPProtocolVersion version;
	union {
		HttpProtocolDataV2 v2;
	} value;
} HttpProtocolData;

#define DEFAULT_RESPONSE_PROTOCOL_DATA \
	((HttpProtocolData){ .version = HTTPProtocolVersion1Dot1, .value = {} })

typedef struct {
	HTTPRequestMethod method;
	ParsedRequestURI uri;
	HttpProtocolData protocol_data;
} HttpRequestLine;

NODISCARD const char* get_http_method_string(HTTPRequestMethod method);

NODISCARD const char* get_http_protocol_version_string(HTTPProtocolVersion protocol_version);

TVEC_DEFINE_VEC_TYPE(HttpHeaderField)

typedef TVEC_TYPENAME(HttpHeaderField) HttpHeaderFields;

typedef struct {
	HttpRequestLine request_line;
	// TODO: are header fileds an array of key value or a hasmpa?
	//  see MAP_INSERT(ParsedSearchPathHashMap, as we treat search params as map
	HttpHeaderFields header_fields;
} HttpRequestHead;

typedef struct {
	HttpRequestHead head;
	SizedBuffer body;
} HttpRequest;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HttpRequestErrorTypeInvalidHttpVersion = 0,
	HttpRequestErrorTypeMethodNotSupported,
	HttpRequestErrorTypeInvalidNonEmptyBody,
	HttpRequestErrorTypeInvalidHttp2Preface,
	HttpRequestErrorTypeLengthRequired,
	HttpRequestErrorTypeProtocolError,
	HttpRequestErrorTypeNotSupported
} HttpRequestErrorType;

typedef struct {
	bool is_advanced;
	union {
		HttpRequestErrorType enum_value;
		const char* advanced;
	} value;
} HttpRequestError;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPPropertyTypeInvalid = 0,
	HTTPPropertyTypeNormal,
	HTTPPropertyTypeOptions,
	HTTPPropertyTypeConnect,
} HTTPPropertyType;

typedef struct {
	HTTPPropertyType type;
	union {
		ParsedURLPath normal; // Method: POST | GET | HEAD
		int todo_options;     // Method: OPTIONS
		int todo_connect;     // Method: CONNECT
	} data;
} HttpRequestProperties;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	CompressionValueTypeNoEncoding = 0,
	CompressionValueTypeAllEncodings,
	CompressionValueTypeNormalEncoding,
} CompressionValueType;

typedef struct {
	CompressionValueType type;
	union {
		CompressionType normal_compression;
	} data;
} CompressionValue;

typedef struct {
	CompressionValue value;
	float weight;
} CompressionEntry;

TVEC_DEFINE_VEC_TYPE(CompressionEntry)

typedef TVEC_TYPENAME(CompressionEntry) CompressionEntries;

typedef struct {
	CompressionEntries entries;
} CompressionSettings;

typedef struct {
	CompressionSettings compression_settings;
	HttpProtocolData protocol_data;
	HttpRequestProperties http_properties;
} RequestSettings;

typedef struct {
	HttpRequest request;
	RequestSettings settings;
} HTTPResultOk;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HttpRequestResultTypeOk = 0,
	HttpRequestResultTypeError,
	HttpRequestResultTypeCloseConnection,
} HttpRequestResultType;

typedef struct {
	HttpRequestResultType type;
	union {
		HTTPResultOk ok;
		HttpRequestError error;
	} value;
} HttpRequestResult;

void free_http_request_head(HttpRequestHead head);

// frees the HttpRequest, taking care of Null Pointer, this is needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void free_http_request(HttpRequest request);

void free_http_request_result(HTTPResultOk result);

NODISCARD const ParsedSearchPathEntry* find_search_key(ParsedSearchPath path, const tstr* key);

// simple helper for getting the status Message for a special status code, not all implemented,
// only the ones needed
NODISCARD const char* get_status_message(HttpStatusCode status_code);

NODISCARD HttpHeaderField* find_header_by_key(HttpHeaderFields array, const tstr* key);

typedef struct {
	CompressionType compression_to_use;
	HttpProtocolData protocol_data;
} SendSettings;

NODISCARD SendSettings get_send_settings(RequestSettings request_settings);

void free_http_header_field(HttpHeaderField field);

void free_http_header_fields(HttpHeaderFields* header_fields);

void add_http_header_field_const_key_dynamic_value(HttpHeaderFields* header_fields, const char* key,
                                                   tstr value);

void add_http_header_field_const_key_const_value(HttpHeaderFields* header_fields, const char* key,
                                                 const char* value);

#define HTTP_LINE_SEPERATORS "\r\n"

#define SIZEOF_HTTP_LINE_SEPERATORS 2

static_assert((sizeof(HTTP_LINE_SEPERATORS) / (sizeof(HTTP_LINE_SEPERATORS[0]))) - 1 ==
              SIZEOF_HTTP_LINE_SEPERATORS);

// TODO: move thos etstr helper functions t the header file
NODISCARD inline tstr tstr_from_static_cstr(const char* const value) {
	const size_t size = strlen(value);
	// cast he const away, as we never alter this pointer, also this doesn't need to be freed
	// afterwards
	const tstr result = tstr_own((char*)value, size, size);

	return result;
}

NODISCARD inline tstr tstr_own_cstr(char* const value) {
	const size_t size = strlen(value);

	const tstr result = tstr_own(value, size, size);

	return result;
}

#ifdef __cplusplus
}
#endif
