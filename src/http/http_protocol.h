

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// needed h files
#include "./compression.h"
#include "./http_2.h"

#include "./uri.h"
#include "utils/log.h"
#include "utils/sized_buffer.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

#include <zvec/zvec.h>

#define STRINGIFY(a) STR_IMPL(a)
#define STR_IMPL(a) #a

// according to https://datatracker.ietf.org/doc/html/rfc7231#section-6.1
// + 418
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	HttpStatusContinue = 100,
	HttpStatusSwitchingProtocols = 101,
	HttpStatusOk = 200,
	HttpStatusCreated = 201,
	HttpStatusAccepted = 202,
	HttpStatusNonAuthoritativeInformation = 203,
	HttpStatusNoContent = 204,
	HttpStatusResetContent = 205,
	HttpStatusPartialContent = 206,
	HttpStatusMultipleChoices = 300,
	HttpStatusMovedPermanently = 301,
	HttpStatusFound = 302,
	HttpStatusSeeOther = 303,
	HttpStatusNotModified = 304,
	HttpStatusUseProxy = 305,
	HttpStatusTemporaryRedirect = 307,
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
	HttpStatusInternalServerError = 500,
	HttpStatusNotImplemented = 501,
	HttpStatusBadGateway = 502,
	HttpStatusServiceUnavailable = 503,
	HttpStatusGatewayTimeout = 504,
	HttpStatusHttpVersionNotSupported = 505,
} HttpStatusCode;

#define FREE_IF_NOT_NULL(pointerToFree) \
	do { \
		if((pointerToFree) != NULL) { \
			free(pointerToFree); \
		} \
	} while(false)

// self implemented Http Request and Http Response Structs

typedef struct {
	char* key;
	char* value;
} HttpHeaderField;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRequestMethodInvalid = 0,
	HTTPRequestMethodGet,
	HTTPRequestMethodPost,
	HTTPRequestMethodHead,
	HTTPRequestMethodOptions,
	HTTPRequestMethodConnect
} HTTPRequestMethod;

#define DEFAULT_RESPONSE_PROTOCOL_VERSION HTTPProtocolVersion1Dot1

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPProtocolVersionInvalid = 0,
	HTTPProtocolVersion1,
	HTTPProtocolVersion1Dot1,
	HTTPProtocolVersion2,
} HTTPProtocolVersion;

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

ZVEC_DEFINE_VEC_TYPE(HttpHeaderField)

typedef ZVEC_TYPENAME(HttpHeaderField) HttpHeaderFields;

typedef struct {
	HttpRequestLine request_line;
	ZVEC_TYPENAME(HttpHeaderField) header_fields;
} HttpRequestHead;

typedef struct {
	HttpResponseLine response_line;
	ZVEC_TYPENAME(HttpHeaderField) header_fields;
} HttpResponseHead;

typedef struct {
	HttpRequestHead head;
	char* body;
} Http1Request;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HttpRequestTypeInternalV1 = 0,
	HttpRequestTypeInternalV1Keepalive,
	HttpRequestTypeInternalV2,
} HttpRequestTypeInternal;

typedef struct {
	int todo;
} Http1RequestKeepaliveContext;

typedef struct {
	int todo;
} Http2RequestContext;

typedef struct {
	HttpRequestTypeInternal type;
	union {
		Http1Request* v1;
		Http1RequestKeepaliveContext* keepalive_v1;
		Http2RequestContext* v2;
	} data;
} HttpRequest;

typedef struct {
	HttpResponseHead head;
	SizedBuffer body;
} HttpResponse;

/*
This is a selfmade http response constructor and request parser, it uses string functions of c and
the self implemented string_builder to achieve this task, it is rudimentary, since it doesn't comply
to the spec exactly, some things are compliant, but not checked if 100% compliant to spec, but thats
not the requirement of this task, it can parse all tested http requests in the right way and also
construct the responses correctly
*/

// frees the HttpRequest, taking care of Null Pointer, this si needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void free_http1_request(Http1Request* request);

void free_http_request(HttpRequest* request);

// returning a stringbuilder, that makes a string from the httpRequest, this is useful for debugging

NODISCARD StringBuilder* http_request_to_string_builder(const HttpRequest* request, bool https);

// if the parsing did go wrong NULL is returned otherwise everything is filled with malloced
// strings, but keep in mind that you gave to use the given free method to free that properly,
// internally some string"magic" happens
NODISCARD HttpRequest* parse_http_request(char* raw_http_request, bool use_http2);

NODISCARD const ParsedSearchPathEntry* find_search_key(ParsedSearchPath path, const char* key);

// simple helper for getting the status Message for a special status code, not all implemented,
// only the ones needed
NODISCARD const char* get_status_message(HttpStatusCode status_code);

NODISCARD HttpHeaderField* find_header_by_key(ZVEC_TYPENAME(HttpHeaderField) array,
                                              const char* key);

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

ZVEC_DEFINE_VEC_TYPE(CompressionEntry)

typedef ZVEC_TYPENAME(CompressionEntry) CompressionEntries;

typedef struct {
	CompressionEntries entries;
} CompressionSettings;

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

typedef struct {
	CompressionSettings* compression_settings;
	HTTPProtocolVersion protocol_used;
	HttpRequestProperties http_properties;
} RequestSettings;

typedef struct {
	CompressionType compression_to_use;
	HTTPProtocolVersion protocol_to_use;
} SendSettings;

NODISCARD CompressionSettings* get_compression_settings(HttpHeaderFields header_fields);

void free_compression_settings(CompressionSettings* compression_settings);

NODISCARD RequestSettings* get_request_settings(HttpRequest* http_request);

void free_request_settings(RequestSettings* request_settings);

NODISCARD SendSettings get_send_settings(RequestSettings* request_settings);

typedef struct {
	StringBuilder* headers;
	SizedBuffer body;
} HttpConcattedResponse;

// makes a string_builder from the HttpResponse, just does the opposite of parsing A Request, but
// with some slight modification
NODISCARD HttpConcattedResponse* http_response_concat(HttpResponse* response);

void free_http_header_fields(HttpHeaderFields* header_fields);

void add_http_header_field_by_double_str(HttpHeaderFields* header_fields, char* double_str);

// free the HttpResponse, just freeing everything necessary
void free_http_response(HttpResponse* response);

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is
// static, but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)

NODISCARD StringBuilder*
html_from_string(StringBuilder* head_content, // NOLINT(bugprone-easily-swappable-parameters)
                 StringBuilder* script_content, StringBuilder* style_content,
                 StringBuilder* body_content);

NODISCARD StringBuilder* http_request_to_json(const HttpRequest* request, bool https,
                                              SendSettings send_settings);

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is
// static, but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)
NODISCARD StringBuilder* http_request_to_html(const HttpRequest* request, bool https,
                                              SendSettings send_settings);

#ifdef __cplusplus
}
#endif
