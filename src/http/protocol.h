

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
	char* key;
	char* value;
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
	HTTPRequestMethod method;
	ParsedRequestURI uri;
	HTTPProtocolVersion protocol_version;
} HttpRequestLine;

NODISCARD const char* get_http_method_string(HTTPRequestMethod method);

NODISCARD const char* get_http_protocol_version_string(HTTPProtocolVersion protocol_version);

typedef struct {
	char* protocol_version;
	char* status_code;
	char* status_message;
} HttpResponseLine;

TVEC_DEFINE_VEC_TYPE(HttpHeaderField)

typedef TVEC_TYPENAME(HttpHeaderField) HttpHeaderFields;

typedef struct {
	HttpRequestLine request_line;
	TVEC_TYPENAME(HttpHeaderField) header_fields;
} HttpRequestHead;

typedef struct {
	HttpResponseLine response_line;
	TVEC_TYPENAME(HttpHeaderField) header_fields;
} HttpResponseHead;

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
	HTTPProtocolVersion protocol_used;
	HttpRequestProperties http_properties;
} RequestSettings;

typedef struct {
	HttpRequest request;
	RequestSettings settings;
} HTTPResultOk;

typedef struct {
	bool is_error;
	union {
		HTTPResultOk result;
		HttpRequestError error;
	} value;
} HttpRequestResult;

typedef struct {
	HttpResponseHead head;
	SizedBuffer body;
} HttpResponse;

// frees the HttpRequest, taking care of Null Pointer, this is needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void free_http_request(HttpRequest request);

void free_http_request_result(HTTPResultOk result);

NODISCARD const ParsedSearchPathEntry* find_search_key(ParsedSearchPath path, const char* key);

// simple helper for getting the status Message for a special status code, not all implemented,
// only the ones needed
NODISCARD const char* get_status_message(HttpStatusCode status_code);

NODISCARD HttpHeaderField* find_header_by_key(TVEC_TYPENAME(HttpHeaderField) array,
                                              const char* key);

typedef struct {
	CompressionType compression_to_use;
	HTTPProtocolVersion protocol_to_use;
} SendSettings;

NODISCARD SendSettings get_send_settings(RequestSettings request_settings);

void free_http_header_fields(HttpHeaderFields* header_fields);

void add_http_header_field_by_double_str(HttpHeaderFields* header_fields, char* double_str);

// free the HttpResponse, just freeing everything necessary
void free_http_response(HttpResponse* response);

typedef struct {
	StringBuilder* headers;
	SizedBuffer body;
} HttpConcattedResponse;

// makes a string_builder from the HttpResponse, just does the opposite of parsing A Request, but
// with some slight modification
NODISCARD HttpConcattedResponse* http_response_concat(HttpResponse* response);

#define HTTP_LINE_SEPERATORS "\r\n"

#define SIZEOF_HTTP_LINE_SEPERATORS 2

static_assert((sizeof(HTTP_LINE_SEPERATORS) / (sizeof(HTTP_LINE_SEPERATORS[0]))) - 1 ==
              SIZEOF_HTTP_LINE_SEPERATORS);

#ifdef __cplusplus
}
#endif
