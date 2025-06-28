

#pragma once

#include <stdlib.h>

// needed h files
#include "utils/log.h"
#include "utils/string_builder.h"
#include "utils/utils.h"
#include <stb/ds.h>

// some Mime Type Definitons:

#define DEFAULT_MIME_TYPE MIME_TYPE_HTML

#define MIME_TYPE_HTML "text/html"
#define MIME_TYPE_JSON "application/json"
#define MIME_TYPE_TEXT "text/plain"

#define STRINGIFY(a) STR_IMPL(a)
#define STR_IMPL(a) #a

// according to https://datatracker.ietf.org/doc/html/rfc7231#section-6.1
// + 418
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	HTTP_STATUS_CONTINUE = 100,
	HTTP_STATUS_SWITCHING_PROTOCOLS = 101,
	HTTP_STATUS_OK = 200,
	HTTP_STATUS_CREATED = 201,
	HTTP_STATUS_ACCEPTED = 202,
	HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
	HTTP_STATUS_NO_CONTENT = 204,
	HTTP_STATUS_RESET_CONTENT = 205,
	HTTP_STATUS_PARTIAL_CONTENT = 206,
	HTTP_STATUS_MULTIPLE_CHOICES = 300,
	HTTP_STATUS_MOVED_PERMANENTLY = 301,
	HTTP_STATUS_FOUND = 302,
	HTTP_STATUS_SEE_OTHER = 303,
	HTTP_STATUS_NOT_MODIFIED = 304,
	HTTP_STATUS_USE_PROXY = 305,
	HTTP_STATUS_TEMPORARY_REDIRECT = 307,
	HTTP_STATUS_BAD_REQUEST = 400,
	HTTP_STATUS_UNAUTHORIZED = 401,
	HTTP_STATUS_PAYMENT_REQUIRED = 402,
	HTTP_STATUS_FORBIDDEN = 403,
	HTTP_STATUS_NOT_FOUND = 404,
	HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
	HTTP_STATUS_NOT_ACCEPTABLE = 406,
	HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
	HTTP_STATUS_REQUEST_TIMEOUT = 408,
	HTTP_STATUS_CONFLICT = 409,
	HTTP_STATUS_GONE = 410,
	HTTP_STATUS_LENGTH_REQUIRED = 411,
	HTTP_STATUS_PRECONDITION_FAILED = 412,
	HTTP_STATUS_PAYLOAD_TOO_LARGE = 413,
	HTTP_STATUS_URI_TOO_LONG = 414,
	HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415,
	HTTP_STATUS_RANGE_NOT_SATISFIABLE = 416,
	HTTP_STATUS_EXPECTATION_FAILED = 417,
	HTTP_STATUS_UPGRADE_REQUIRED = 426,
	HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
	HTTP_STATUS_NOT_IMPLEMENTED = 501,
	HTTP_STATUS_BAD_GATEWAY = 502,
	HTTP_STATUS_SERVICE_UNAVAILABLE = 503,
	HTTP_STATUS_GATEWAY_TIMEOUT = 504,
	HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505,
} HTTP_STATUS_CODES;

#define freeIfNotNULL(pointerToFree) \
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

typedef struct {
	char* method;
	char* URI;
	char* protocolVersion;
} HttpRequestLine;

typedef struct {
	char* protocolVersion;
	char* statusCode;
	char* statusMessage;
} HttpResponseLine;

typedef struct {
	HttpRequestLine requestLine;
	STBDS_ARRAY(HttpHeaderField) headerFields;
} HttpRequestHead;

typedef struct {
	HttpResponseLine responseLine;
	STBDS_ARRAY(HttpHeaderField) headerFields;
} HttpResponseHead;

typedef struct {
	HttpRequestHead head;
	char* body;
} HttpRequest;

typedef struct {
	HttpResponseHead head;
	char* body;
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
void freeHttpRequest(HttpRequest* request);
// returning a stringbuilder, that makes a string from the httpRequest, this is useful for debugging

StringBuilder* httpRequestToStringBuilder(HttpRequest* request, bool https);

// if the parsing did go wrong NULL is returned otherwise everything is filled with malloced
// strings, but keep in mind that you gave to use the given free method to free that properly,
// internally some string"magic" happens
HttpRequest* parseHttpRequest(char* rawHttpRequest);

// simple helper for getting the status Message for a special status code, not all implemented,
// only the ones needed
const char* getStatusMessage(int statusCode);

// simple http Response constructor using string builder, headers can be NULL, when headerSize is
// also null!
HttpResponse* constructHttpResponseWithHeaders(int status, char* body,
                                               HttpHeaderField* additionalHeaders,
                                               size_t headersSize, const char* MIMEType);

// wrapper if no additionalHeaders are required
HttpResponse* constructHttpResponse(int status, char* body, const char* MIMEType);

// makes a stringBuilder from the HttpResponse, just does the opposite of parsing A Request, but
// with some slight modification
StringBuilder* httpResponseToStringBuilder(HttpResponse* response);

// free the HttpResponse, just freeing everything necessary
void freeHttpResponse(HttpResponse* response);

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is static,
// but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)

char* htmlFromString(char* headContent, char* scriptContent, char* styleContent, char* bodyContent);

char* httpRequestToJSON(HttpRequest* request, bool https);

char* httpRequestToHtml(HttpRequest* request, bool https);
