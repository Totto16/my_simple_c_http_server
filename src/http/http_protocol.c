
#include "http_protocol.h"
#include <ctype.h>
#include <math.h>

NODISCARD static HTTPRequestMethod getMethodFromString(char* method) {

	if(strcmp(method, "GET") == 0) {
		return HTTPRequestMethodGet;
	}

	if(strcmp(method, "POST") == 0) {
		return HTTPRequestMethodPost;
	}

	if(strcmp(method, "HEAD") == 0) {
		return HTTPRequestMethodHead;
	}

	if(strcmp(method, "OPTIONS") == 0) {
		return HTTPRequestMethodOptions;
	}

	return HTTPRequestMethodInvalid;
}

NODISCARD static HTTPProtocolVersion getProtocolVersionFromString(char* protocolVersion) {

	if(strcmp(protocolVersion, "HTTP/1.1") == 0) {
		return HTTPProtocolVersion_1_1;
	}

	if(strcmp(protocolVersion, "HTTP/1.0") == 0) {
		return HTTPProtocolVersion_1;
	}

	if(strcmp(protocolVersion, "HTTP/2") == 0) {
		return HTTPProtocolVersion_2;
	}

	return HTTPProtocolVersionInvalid;
}

NODISCARD HttpRequestLine getRequestLineFromRawLine(HttpRawRequestLine line) {

	HttpRequestLine result = { .URI = line.URI };

	result.method = getMethodFromString(line.method);

	result.protocolVersion = getProtocolVersionFromString(line.protocolVersion);

	return result;
}

static void freeRawRequestLine(HttpRawRequestLine line) {
	// frees all 3 fields, as they are one allocation, with isnmerted 0 bytes
	free(line.method);
}

// frees the HttpRequest, taking care of Null Pointer, this si needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void freeHttpRequest(HttpRequest* request) {
	freeRawRequestLine(request->head.rawRequestLine);
	for(size_t i = 0; i < stbds_arrlenu(request->head.headerFields); ++i) {
		// same elegant freeing but two at once :)
		freeIfNotNULL(request->head.headerFields[i].key);
	}
	stbds_arrfree(request->head.headerFields);
	freeIfNotNULL(request->body);
	freeIfNotNULL(request);
}

// returning a stringbuilder, that makes a string from the httpRequest, this is useful for debugging
StringBuilder* httpRequestToStringBuilder(const HttpRequest* const request, bool https) {
	StringBuilder* result = string_builder_init();
	string_builder_append_single(result, "HttpRequest:\n");
	string_builder_append(result, return NULL;
	                      , "\tMethod: %s\n", request->head.rawRequestLine.method);
	string_builder_append(result, return NULL;, "\tURI: %s\n", request->head.rawRequestLine.URI);
	string_builder_append(result, return NULL;, "\tProtocolVersion : %s\n",
	                                          request->head.rawRequestLine.protocolVersion);

	string_builder_append(result, return NULL;, "\tSecure : %s\n", https ? "true" : " false");

	for(size_t i = 0; i < stbds_arrlenu(request->head.headerFields); ++i) {
		// same elegant freeing but wo at once :)
		string_builder_append(result, return NULL;, "\tHeader:\n\t\tKey: %s \n\t\tValue: %s\n",
		                                          request->head.headerFields[i].key,
		                                          request->head.headerFields[i].value);
	}
	string_builder_append(result, return NULL;, "\tBody: %s\n", request->body);
	return result;
}

// if the parsing did go wrong NULL is returned otherwise everything is filled with malloced
// strings, but keep in mind that you gave to use the given free method to free that properly,
// internally some string"magic" happens
HttpRequest* parseHttpRequest(char* rawHttpRequest) {

	// considered using strtok, but that doesn't recognize the delimiter between the status and
	// body! so now using own way of doing that!

	const char* const separators = "\r\n";
	size_t separatorsLength = strlen(separators);
	char* currentlyAt = rawHttpRequest;
	bool parsed = false;
	HttpRequest* request = (HttpRequest*)mallocWithMemset(sizeof(HttpRequest), true);

	if(!request) {
		return NULL;
	}

	STBDS_ARRAY_INIT(request->head.headerFields);

	// iterating over each separated string, then determining if header or body or statusLine and
	// then parsing that accordingly
	do {
		char* resultingIndex = strstr(currentlyAt, separators);
		// no"\r\n" could be found, so a parse Error occurred, a NULL signals that
		if(resultingIndex == NULL) {
			// also the input rawHttpRequest string has to be freed
			free(rawHttpRequest);
			// no more possible leaks, since some fields may be initialized, is covered by the
			// freeHttpRequest implementation
			freeHttpRequest(request);
			return NULL;
		}
		char* all = (char*)mallocWithMemset(resultingIndex - currentlyAt + 1, true);

		if(!all) {
			return NULL;
		}

		// return pointer == all, so is ignored
		memcpy(all, currentlyAt, resultingIndex - currentlyAt);

		// other way of checking if at the beginning
		if(currentlyAt == rawHttpRequest) {
			// parsing the string and inserting"\0" bytes at the" " space byte, so the three part
			// string can be used in three different fields, with the correct start address, this
			// trick is used more often trough-out this implementation, you don't have to understand
			// it, since its abstracted away when using only the provided function
			char* begin = index(all, ' ');
			*begin = '\0';
			request->head.rawRequestLine.method = all;
			all = begin + 1;
			begin = index(all, ' ');
			*begin = '\0';
			request->head.rawRequestLine.URI = all;
			all = begin + 1;
			// is already null terminated!
			request->head.rawRequestLine.protocolVersion = all;

			request->head.requestLine = getRequestLineFromRawLine(request->head.rawRequestLine);

		} else {
			if(strlen(all) == 0) {
				// that denotes now comes the body! so the body is assigned and the loop ends with
				// the parsed = true the while loop finishes
				free(all);
				size_t bodyLength =
				    strlen(rawHttpRequest) - ((resultingIndex - rawHttpRequest) + separatorsLength);
				all = (char*)mallocWithMemset(bodyLength + 1, true);

				if(!all) {
					LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation,
					                   "Couldn't allocate memory!\n");
					return NULL;
				}

				memcpy(all, currentlyAt + separatorsLength, bodyLength);
				request->body = all;
				parsed = true;
			} else {
				// here headers are parsed, here":" is the delimiter

				// using same trick, the header string is one with the right 0 bytes :)
				char* begin = index(all, ':');
				*begin = '\0';
				if(*(begin + 1) == ' ') {
					++begin;
					*begin = '\0';
				}

				size_t current_array_index = stbds_arrlenu(request->head.headerFields);

				stbds_arrsetlen(request->head.headerFields, current_array_index + 1);

				request->head.headerFields[current_array_index].key = all;
				request->head.headerFields[current_array_index].value = begin + 1;
			}
		}

		// adjust the values
		currentlyAt = resultingIndex + separatorsLength;

	} while(!parsed);

	// at the end free the input rawHttpRequest string
	free(rawHttpRequest);
	return request;
}

// simple helper for getting the status Message for a special status code, all from the spec for
// http 1.1 implemented (not in the spec e.g. 418)
const char* getStatusMessage(HTTP_STATUS_CODES statusCode) {
	const char* result = "NOT SUPPORTED STATUS CODE"; // NOLINT(clang-analyzer-deadcode.DeadStores)
	// according to https://datatracker.ietf.org/doc/html/rfc7231#section-6.1
	switch(statusCode) {
		case HTTP_STATUS_CONTINUE: result = "Continue"; break;
		case HTTP_STATUS_SWITCHING_PROTOCOLS: result = "Switching Protocols"; break;
		case HTTP_STATUS_OK: result = "OK"; break;
		case HTTP_STATUS_CREATED: result = "Created"; break;
		case HTTP_STATUS_ACCEPTED: result = "Accepted"; break;
		case HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
			result = "Non-Authoritative Information";
			break;
		case HTTP_STATUS_NO_CONTENT: result = "No Content"; break;
		case HTTP_STATUS_RESET_CONTENT: result = "Reset Content"; break;
		case HTTP_STATUS_PARTIAL_CONTENT: result = "Partial Content"; break;
		case HTTP_STATUS_MULTIPLE_CHOICES: result = "Multiple Choices"; break;
		case HTTP_STATUS_MOVED_PERMANENTLY: result = "Moved Permanently"; break;
		case HTTP_STATUS_FOUND: result = "Found"; break;
		case HTTP_STATUS_SEE_OTHER: result = "See Other"; break;
		case HTTP_STATUS_NOT_MODIFIED: result = "Not Modified"; break;
		case HTTP_STATUS_USE_PROXY: result = "Use Proxy"; break;
		case HTTP_STATUS_TEMPORARY_REDIRECT: result = "Temporary Redirect"; break;
		case HTTP_STATUS_BAD_REQUEST: result = "Bad Request"; break;
		case HTTP_STATUS_UNAUTHORIZED: result = "Unauthorized"; break;
		case HTTP_STATUS_PAYMENT_REQUIRED: result = "Payment Required"; break;
		case HTTP_STATUS_FORBIDDEN: result = "Forbidden"; break;
		case HTTP_STATUS_NOT_FOUND: result = "Not Found"; break;
		case HTTP_STATUS_METHOD_NOT_ALLOWED: result = "Method Not Allowed"; break;
		case HTTP_STATUS_NOT_ACCEPTABLE: result = "Not Acceptable"; break;
		case HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED:
			result = "Proxy Authentication Required";
			break;
		case HTTP_STATUS_REQUEST_TIMEOUT: result = "Request Timeout"; break;
		case HTTP_STATUS_CONFLICT: result = "Conflict"; break;
		case HTTP_STATUS_GONE: result = "Gone"; break;
		case HTTP_STATUS_LENGTH_REQUIRED: result = "Length Required"; break;
		case HTTP_STATUS_PRECONDITION_FAILED: result = "Precondition Failed"; break;
		case HTTP_STATUS_PAYLOAD_TOO_LARGE: result = "Payload Too Large"; break;
		case HTTP_STATUS_URI_TOO_LONG: result = "URI Too Long"; break;
		case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE: result = "Unsupported Media Type"; break;
		case HTTP_STATUS_RANGE_NOT_SATISFIABLE: result = "Range Not Satisfiable"; break;
		case HTTP_STATUS_EXPECTATION_FAILED: result = "Expectation Failed"; break;
		case HTTP_STATUS_UPGRADE_REQUIRED: result = "Upgrade Required"; break;
		case HTTP_STATUS_INTERNAL_SERVER_ERROR: result = "Internal Server Error"; break;
		case HTTP_STATUS_NOT_IMPLEMENTED: result = "Not Implemented"; break;
		case HTTP_STATUS_BAD_GATEWAY: result = "Bad Gateway"; break;
		case HTTP_STATUS_SERVICE_UNAVAILABLE: result = "Service Unavailable"; break;
		case HTTP_STATUS_GATEWAY_TIMEOUT: result = "Gateway Timeout"; break;
		case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED: result = "HTTP Version Not Supported"; break;
		default: break;
	}
	return result;
}

NODISCARD static HttpHeaderField* find_header_by_key(HttpHeaderFields array, const char* key) {

	for(size_t i = 0; i < stbds_arrlenu(array); ++i) {
		HttpHeaderField* header = &(array[i]);
		if(strcasecmp(header->key, key) == 0) {
			return header;
		}
	}

	return NULL;
}

static COMPRESSION_TYPE parse_compression_type(char* compression_name, bool* ok_result) {
	if(strcmp(compression_name, "gzip") == 0) {
		*ok_result = true;
		return COMPRESSION_TYPE_GZIP;
	}

	if(strcmp(compression_name, "deflate") == 0) {
		*ok_result = true;
		return COMPRESSION_TYPE_DEFLATE;
	}

	if(strcmp(compression_name, "br") == 0) {
		*ok_result = true;
		return COMPRESSION_TYPE_BR;
	}

	if(strcmp(compression_name, "zstd") == 0) {
		*ok_result = true;
		return COMPRESSION_TYPE_ZSTD;
	}

	LOG_MESSAGE(LogLevelWarn, "Not recognized compression level: %s\n", compression_name);

	*ok_result = false;
	return COMPRESSION_TYPE_NONE;
}

static CompressionValue parse_compression_value(char* compression_name, bool* ok_result) {

	if(strcmp(compression_name, "*") == 0) {
		*ok_result = true;
		return (CompressionValue){ .type = CompressionValueType_ALL_ENCODINGS };
	}

	if(strcmp(compression_name, "identity") == 0) {
		*ok_result = true;
		return (CompressionValue){ .type = CompressionValueType_NO_ENCODING };
	}

	CompressionValue result = { .type = CompressionValueType_NORMAL_ENCODING };

	COMPRESSION_TYPE type = parse_compression_type(compression_name, ok_result);

	if(!(*ok_result)) {
		return result;
	}

	result.data.normal_compression = type;
	*ok_result = true;

	return result;
}

static CompressionSettings* getCompressionSettings(HttpRequest* httpRequest) {

	CompressionSettings* compressionSettings =
	    (CompressionSettings*)mallocWithMemset(sizeof(CompressionSettings), true);

	if(!compressionSettings) {
		return NULL;
	}

	STBDS_ARRAY_INIT(compressionSettings->entries);

	// see: https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

	HttpHeaderField* acceptEncodingHeader =
	    find_header_by_key(httpRequest->head.headerFields, "accept-encoding");

	if(!acceptEncodingHeader) {
		return compressionSettings;
	}

	char* raw_value = acceptEncodingHeader->value;

	if(strlen(raw_value) == 0) {
		return compressionSettings;
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

			CompressionEntry entry = { .weight = 1.0F };

			if(compression_weight != NULL) {
				float value = parseFloat(compression_weight);

				if(!isnan(value)) {
					entry.weight = value;
				}
			}

			bool ok_result = true;
			CompressionValue comp_value = parse_compression_value(compression_name, &ok_result);

			if(ok_result) {
				entry.value = comp_value;

				stbds_arrput(compressionSettings->entries, entry);
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
	return compressionSettings;
}

void freeCompressionSettings(CompressionSettings* compressionSettings) {
	stbds_arrfree(compressionSettings->entries);
	free(compressionSettings);
}

RequestSettings* getRequestSettings(HttpRequest* httpRequest) {

	RequestSettings* requestSettings =
	    (RequestSettings*)mallocWithMemset(sizeof(RequestSettings), true);

	if(!requestSettings) {
		return NULL;
	}

	CompressionSettings* compressionSettings = getCompressionSettings(httpRequest);

	if(!compressionSettings) {
		free(requestSettings);
		return NULL;
	}

	requestSettings->compression_settings = compressionSettings;

	return requestSettings;
}

void freeRequestSettings(RequestSettings* requestSettings) {

	freeCompressionSettings(requestSettings->compression_settings);
	free(requestSettings);
}

#define COMPRESSIONS_SIZE 4

static COMPRESSION_TYPE get_best_compression_that_is_supported(void) {

	// This are sorted by compression ratio, not by speed , but this may be inaccurate
	COMPRESSION_TYPE supported_compressions[COMPRESSIONS_SIZE] = {
		COMPRESSION_TYPE_BR,
		COMPRESSION_TYPE_ZSTD,
		COMPRESSION_TYPE_GZIP,
		COMPRESSION_TYPE_DEFLATE,
	};

	for(size_t i = 0; i < COMPRESSIONS_SIZE; ++i) {
		COMPRESSION_TYPE compression = supported_compressions[i];
		if(is_compressions_supported(compression)) {
			return compression;
		}
	}

	return COMPRESSION_TYPE_NONE;
}

static int compare_function_entries(const anyType(CompressionEntry) _entry1,
                                    const anyType(CompressionEntry) _entry2) {
	const CompressionEntry* entry1 = (CompressionEntry*)_entry1;
	const CompressionEntry* entry2 = (CompressionEntry*)_entry2;

	// note weight is between 0.0 and 1.0

	if(entry1->weight != entry2->weight) {
		return (
		    int)((entry1->weight - entry2->weight) *
		         10000.0F); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	return 0;
}

SendSettings getSendSettings(RequestSettings* requestSettings) {

	SendSettings result = { .compression_to_use = COMPRESSION_TYPE_NONE };

	CompressionEntries entries = requestSettings->compression_settings->entries;

	size_t entries_length = stbds_arrlenu(entries);

	if(entries_length == 0) {
		return result;
	}

	// this sorts the entries by weight, same weight means, we prefer the ones that come first in
	// the string, as it is unspecified in the spec, on what to sort as 2. criterium
	qsort(entries, entries_length, sizeof(CompressionEntry), compare_function_entries);

	for(size_t i = 0; i < entries_length; ++i) {
		CompressionEntry entry = entries[i];

		switch(entry.value.type) {
			case CompressionValueType_NO_ENCODING: {
				result.compression_to_use = COMPRESSION_TYPE_NONE;
				goto break_for;
			}
			case CompressionValueType_ALL_ENCODINGS: {
				result.compression_to_use = get_best_compression_that_is_supported();
				goto break_for;
			}
			case CompressionValueType_NORMAL_ENCODING: {
				if(is_compressions_supported(entry.value.data.normal_compression)) {
					result.compression_to_use = entry.value.data.normal_compression;
					goto break_for;
				}
				break;
			}
			default: {
				result.compression_to_use = COMPRESSION_TYPE_NONE;
				goto break_for;
			}
		}
	}
break_for:

	return result;
}

// makes a stringBuilder + a sized body from the HttpResponse, just does the opposite of parsing a
// Request, but with some slight modification
HttpConcattedResponse* httpResponseConcat(HttpResponse* response) {
	HttpConcattedResponse* concattedResponse =
	    (HttpConcattedResponse*)mallocWithMemset(sizeof(HttpConcattedResponse), true);

	if(!concattedResponse) {
		return NULL;
	}

	StringBuilder* result = string_builder_init();
	const char* const separators = "\r\n";

	string_builder_append(result, return NULL;
	                      , "%s %s %s%s", response->head.responseLine.protocolVersion,
	                      response->head.responseLine.statusCode,
	                      response->head.responseLine.statusMessage, separators);

	for(size_t i = 0; i < stbds_arrlenu(response->head.headerFields); ++i) {
		// same elegant freeing but two at once :)
		string_builder_append(result, return NULL;, "%s: %s%s", response->head.headerFields[i].key,
		                                          response->head.headerFields[i].value, separators);
	}

	string_builder_append_single(result, separators);

	concattedResponse->headers = result;
	concattedResponse->body = response->body;

	return concattedResponse;
}

// free the HttpResponse, just freeing everything necessary
void freeHttpResponse(HttpResponse* response) {
	// elegantly freeing three at once :)
	free(response->head.responseLine.protocolVersion);
	for(size_t i = 0; i < stbds_arrlenu(response->head.headerFields); ++i) {
		// same elegant freeing but two at once :)

		free(response->head.headerFields[i].key);
	}
	stbds_arrfree(response->head.headerFields);

	freeSizedBuffer(response->body);

	free(response);
}

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is static,
// but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)

NODISCARD static StringBuilder* htmlFromString(StringBuilder* headContent,
                                               StringBuilder* scriptContent,
                                               StringBuilder* styleContent,
                                               StringBuilder* bodyContent) {

	StringBuilder* result = string_builder_init();

	string_builder_append_single(result, "<!DOCTYPE html><html>");
	string_builder_append_single(result, "<head>");
	string_builder_append_single(result, "<meta charset=\"UTF-8\">");
	string_builder_append_single(
	    result, "<meta name=\"description\" content=\"HTML generated by simple C Http Server\">");
	string_builder_append_single(result, "<meta name=\"author\" content=\"Totto16\">");
	string_builder_append_single(result, "<title>Page by Simple C Http Server</title>");
	if(headContent != NULL) {
		string_builder_append_string_builder(result, headContent);
	}
	if(scriptContent != NULL) {
		string_builder_append_single(result, "<script type=\"text/javascript\">");

		string_builder_append_string_builder(result, scriptContent);
		string_builder_append_single(result, "</script>");
		string_builder_append_single(
		    result,
		    "<noscript> Diese Seite Benötigt Javascript um zu funktionieren :( </noscript>");
	}
	if(styleContent != NULL) {
		string_builder_append_single(result, "<style type=\"text/css\">");
		string_builder_append_string_builder(result, styleContent);
		string_builder_append_single(result, "</style>");
	}
	string_builder_append_single(result, "</head>");
	string_builder_append_single(result, "<body>");
	if(bodyContent != NULL) {
		string_builder_append_string_builder(result, bodyContent);
	}
	string_builder_append_single(result, "</body>");
	string_builder_append_single(result, "</html>");

	return result;
}

StringBuilder* httpRequestToJSON(const HttpRequest* const request, bool https,
                                 SendSettings send_settings) {
	StringBuilder* body = string_builder_init();
	string_builder_append(body, return NULL;
	                      , "{\"request\":\"%s\",", request->head.rawRequestLine.method);
	string_builder_append(body, return NULL;, "\"URI\": \"%s\",", request->head.rawRequestLine.URI);
	string_builder_append(body, return NULL;
	                      , "\"version\":\"%s\",", request->head.rawRequestLine.protocolVersion);
	string_builder_append(body, return NULL;, "\"secure\":%s,", https ? "true" : "false");
	string_builder_append_single(body, "\"headers\":[");

	const size_t headerAmount = stbds_arrlenu(request->head.headerFields);

	for(size_t i = 0; i < headerAmount; ++i) {
		// same elegant freeing but wo at once :)
		string_builder_append(body, return NULL;, "{\"header\":\"%s\", \"key\":\"%s\"}",
		                                        request->head.headerFields[i].key,
		                                        request->head.headerFields[i].value);
		if(i + 1 < headerAmount) {
			string_builder_append_single(body, ", ");
		} else {
			string_builder_append_single(body, "],");
		}
	}
	string_builder_append(body, return NULL;, "\"body\":\"%s\"", request->body);

	string_builder_append_single(body, ", \"settings\": {");

	string_builder_append(body, return NULL;
	                      , "\"send_settings\":{\"compression\" : \"%s\"} }",
	                      get_string_for_compress_format(send_settings.compression_to_use));

	string_builder_append_single(body, "}");
	return body;
}

StringBuilder* httpRequestToHtml(const HttpRequest* const request, bool https,
                                 SendSettings send_settings) {
	StringBuilder* body = string_builder_init();
	string_builder_append_single(body, "<h1 id=\"title\">HttpRequest:</h1><br>");
	string_builder_append(body, return NULL;, "<div id=\"request\"><div>Method: %s</div>",
	                                        request->head.rawRequestLine.method);
	string_builder_append(body, return NULL;
	                      , "<div>URI: %s</div>", request->head.rawRequestLine.URI);
	string_builder_append(body, return NULL;, "<div>ProtocolVersion : %s</div>",
	                                        request->head.rawRequestLine.protocolVersion);
	string_builder_append(body, return NULL;
	                      ,
	                      "<div>Secure : %s</div><button id=\"shutdown\"> Shutdown </button></div>",
	                      https ? "true" : "false");
	string_builder_append_single(body, "<div id=\"header\">");
	for(size_t i = 0; i < stbds_arrlenu(request->head.headerFields); ++i) {
		// same elegant freeing but wo at once :)
		string_builder_append(
		    body, return NULL;
		    , "<div><h2>Header:</h2><br><h3>Key:</h3> %s<br><h3>Value:</h3> %s</div>",
		    request->head.headerFields[i].key, request->head.headerFields[i].value);
	}

	string_builder_append_single(body, "</div> <div id=\"settings\">");
	string_builder_append_single(body, "<h1>Settings:</h1> <br>");
	{
		string_builder_append_single(body, "</div> <div id=\"send_settings\">");
		string_builder_append_single(body, "<h2>Send Settings:</h2> <br>");
		string_builder_append(body, return NULL;
		                      , "<h3>Compression:</h3> %s",
		                      get_string_for_compress_format(send_settings.compression_to_use));
		string_builder_append_single(body, "</div>");
	}
	string_builder_append_single(body, "</div>");

	string_builder_append_single(body, "</div> <div id=\"body\">");
	string_builder_append(body, return NULL;, "<h1>Body:</h1> <br>%s", request->body);
	string_builder_append_single(body, "</div>");

	// style

	StringBuilder* style = string_builder_init();
	string_builder_append_single(
	    style,
	    "body{background: linear-gradient( 90deg, rgb(255, 0, 0) 0%, rgb(255, 154, 0) 10%, "
	    "rgb(208, 222, 33) 20%, rgb(79, 220, 74) 30%, rgb(63, 218, 216) 40%, rgb(47, 201, "
	    "226) 50%, rgb(28, 127, 238) 60%, rgb(95, 21, 242) 70%, rgb(186, 12, 248) 80%, "
	    "rgb(251, 7, 217) 90%, rgb(255, 0, 0) 100% );}"
	    "#request {display: flex;justify-content: center;gap: 5%;color: #1400ff;text-align: "
	    "center;align-items: center;}"
	    "#header {display:flex; flex-direction: column;align-items: center;overflow-wrap: "
	    "anywhere;text-align: center;word-wrap: anywhere;}"
	    "#body {padding: 1%;text-align: center;border: solid 4px black;margin: 1%;}"
	    "#shutdown {border: none;cursor: crosshair;opacity: .9;padding: 16px "
	    "20px;background-color: #c7ff00;font-weight: 900;color: #000;}"
	    "#title{text-align: center;}"
	    "#settings {display:flex; flex-direction: column;align-items: center;overflow-wrap: "
	    "anywhere;text-align: center;word-wrap: anywhere;}"
	    "#send_settings {display:flex; flex-direction: column;align-items: center;overflow-wrap: "
	    "anywhere;text-align: center;word-wrap: anywhere;}"

	);

	// script
	StringBuilder* script = string_builder_init();
	string_builder_append_single(
	    script, "function requestShutdown(){"
	            "	document.querySelector('button').onclick = ()=>{"
	            "		let shutdownUrl = `${location.protocol}//${location.host}/shutdown`;"
	            "		const xhr = new XMLHttpRequest();"
	            "		xhr.onload = (response)=>{"
	            "			if(xhr.status == 200){"
	            "				alert(\"Successfully Shut Down the server!\");"
	            "			}else{"
	            "				console.error(xhr);"
	            "				alert(\"Couldn't Shut Down the Server!\");"
	            "			}"
	            "		};"
	            "		xhr.onerror = (err)=>{"
	            "			console.error(err);"
	            "			alert(\"Couldn't Shut Down the Server!\");"
	            "		};"
	            "		xhr.open(\"GET\", shutdownUrl);"
	            "		xhr.send();"
	            "	}"
	            "}"
	            "window.addEventListener('DOMContentLoaded',requestShutdown);");

	StringBuilder* htmlResult = htmlFromString(NULL, script, style, body);
	free_string_builder(body);
	free_string_builder(style);
	free_string_builder(script);
	return htmlResult;
}
