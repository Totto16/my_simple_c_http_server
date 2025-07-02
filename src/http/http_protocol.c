
#include "http_protocol.h"
#include <ctype.h>
#include <math.h>

NODISCARD static HTTPRequestMethod get_http_method_from_string(const char* method) {

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

NODISCARD static HTTPProtocolVersion
get_protocol_version_from_string(const char* protocol_version) {

	if(strcmp(protocol_version, "HTTP/1.0") == 0) {
		return HTTPProtocolVersion1;
	}

	if(strcmp(protocol_version, "HTTP/1.1") == 0) {
		return HTTPProtocolVersion1Dot1;
	}

	if(strcmp(protocol_version, "HTTP/2") == 0) {
		return HTTPProtocolVersion2;
	}

	return HTTPProtocolVersionInvalid;
}

NODISCARD const char* get_http_method_string(HTTPRequestMethod method) {
	switch(method) {
		case HTTPRequestMethodInvalid: return "<Invalid>";
		case HTTPRequestMethodGet: return "Get";
		case HTTPRequestMethodPost: return "Post";
		case HTTPRequestMethodHead: return "Head";
		case HTTPRequestMethodOptions: return "Options";
		default: return "<Unknown>";
	}
}

NODISCARD char* get_http_url_path_string(ParsedURLPath path) {

	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, path.path);

	size_t search_path_length = stbds_shlenu(path.search_path.hash_map);

	if(search_path_length != 0) {
		string_builder_append_single(string_builder, "?");
	}

	for(size_t i = 0; i < search_path_length; ++i) {
		ParsedSearchPathEntry entry = path.search_path.hash_map[i];

		string_builder_append_single(string_builder, entry.key);

		if(strlen(entry.value) != 0) {
			string_builder_append_single(string_builder, "=");

			string_builder_append_single(string_builder, entry.value);
		}

		if(i != search_path_length - 1) {
			string_builder_append_single(string_builder, "&");
		}
	}

	return string_builder_release_into_string(&string_builder);
}

NODISCARD const char* get_http_protocol_version_string(HTTPProtocolVersion protocol_version) {

	switch(protocol_version) {
		case HTTPProtocolVersionInvalid: return "<Invalid>";
		case HTTPProtocolVersion1: return "HTTP/1.0";
		case HTTPProtocolVersion1Dot1: return "HTTP/1.1";
		case HTTPProtocolVersion2: return "HTTP/2";
		default: return "<Unknown>";
	}
}

/**
 * @brief Get the parsed url path from raw object, it modifies the string inline and creates copies
 * for the result
 *
 * @param path
 * @return NODISCARD
 */
NODISCARD static ParsedURLPath get_parsed_url_path_from_raw(const char* path) {

	if(strlen(path) == 0) {
		path = "/";
	}

	char* search_path = strchr(path, '?');

	ParsedURLPath result = { .search_path = { .hash_map = STBDS_HASM_MAP_EMPTY } };

	if(search_path == NULL) {
		result.path = strdup(path);

		return result;
	}

	*search_path = '\0';

	result.path = strdup(path);

	char* search_params = search_path + 1;

	if(strlen(search_params) == 0) {
		return result;
	}

	while(true) {

		char* next_argument = strchr(search_params, '&');

		if(next_argument != NULL) {
			*next_argument = '\0';
		}

		char* key = search_params;

		char* value_ptr = strchr(search_params, '=');

		if(value_ptr != NULL) {
			*value_ptr = '\0';
		}

		const char* value = value_ptr == NULL ? "" : value_ptr + 1;

		char* key_dup = strdup(key);
		char* value_dup = strdup(value);

		stbds_shput(result.search_path.hash_map, key_dup, value_dup);

		if(next_argument == NULL) {
			break;
		}

		search_params = next_argument + 1;
	}

	return result;
}

NODISCARD static HttpRequestLine
get_request_line_from_raw(const char* method,
                          const char* path, // NOLINT(bugprone-easily-swappable-parameters)
                          const char* protocol_version) {

	HttpRequestLine result = {};

	result.path = get_parsed_url_path_from_raw(path);

	result.method = get_http_method_from_string(method);

	result.protocol_version = get_protocol_version_from_string(protocol_version);

	return result;
}

static void free_parsed_url_path(ParsedURLPath path) {
	free(path.path);

	for(size_t i = 0; i < stbds_shlenu(path.search_path.hash_map); ++i) {
		ParsedSearchPathEntry entry = path.search_path.hash_map[i];

		free(entry.key);
		free(entry.value);
	}

	stbds_shfree(path.search_path.hash_map);
}

static void free_http_request_line(HttpRequestLine line) {
	free_parsed_url_path(line.path);
}

static void free_request_head(HttpRequestHead head) {
	free_http_request_line(head.request_line);
	for(size_t i = 0; i < stbds_arrlenu(head.header_fields); ++i) {
		// same elegant freeing but two at once :)
		FREE_IF_NOT_NULL(head.header_fields[i].key);
	}
	stbds_arrfree(head.header_fields);
}

// frees the HttpRequest, taking care of Null Pointer, this si needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void free_http_request(HttpRequest* request) {
	free_request_head(request->head);
	FREE_IF_NOT_NULL(request->body);
	FREE_IF_NOT_NULL(request);
}

// returning a stringbuilder, that makes a string from the http_request, this is useful for
// debugging
StringBuilder* http_request_to_string_builder(const HttpRequest* const request, bool https) {
	StringBuilder* result = string_builder_init();

	const char* method = get_http_method_string(request->head.request_line.method);
	char* path = get_http_url_path_string(request->head.request_line.path);
	const char* protocol_version =
	    get_http_protocol_version_string(request->head.request_line.protocol_version);

	string_builder_append_single(result, "HttpRequest:\n");

	string_builder_append_single(result, "\tMethod:");
	string_builder_append_single(result, method);
	string_builder_append_single(result, "\n");

	STRING_BUILDER_APPENDF(result, return NULL;, "\tPath: %s\n", path);
	free(path);

	string_builder_append_single(result, "\tProtocolVersion:");
	string_builder_append_single(result, protocol_version);
	string_builder_append_single(result, "\n");

	STRING_BUILDER_APPENDF(result, return NULL;, "\tSecure : %s\n", https ? "true" : " false");

	for(size_t i = 0; i < stbds_arrlenu(request->head.header_fields); ++i) {
		// same elegant freeing but wo at once :)
		STRING_BUILDER_APPENDF(result, return NULL;, "\tHeader:\n\t\tKey: %s \n\t\tValue: %s\n",
		                                           request->head.header_fields[i].key,
		                                           request->head.header_fields[i].value);
	}
	STRING_BUILDER_APPENDF(result, return NULL;, "\tBody: %s\n", request->body);
	return result;
}

// if the parsing did go wrong NULL is returned otherwise everything is filled with malloced
// strings, but keep in mind that you gave to use the given free method to free that properly,
// internally some string"magic" happens
HttpRequest* parse_http_request(char* raw_http_request) {

	// considered using strtok, but that doesn't recognize the delimiter between the status and
	// body! so now using own way of doing that!

	const char* const separators = "\r\n";
	size_t separators_length = strlen(separators);
	char* currently_at = raw_http_request;
	bool parsed = false;
	HttpRequest* request = (HttpRequest*)malloc_with_memset(sizeof(HttpRequest), true);

	if(!request) {
		return NULL;
	}

	request->head.header_fields = STBDS_ARRAY_EMPTY;

	// iterating over each separated string, then determining if header or body or statusLine and
	// then parsing that accordingly
	do {
		char* resulting_index = strstr(currently_at, separators);
		// no"\r\n" could be found, so a parse Error occurred, a NULL signals that
		if(resulting_index == NULL) {
			// also the input raw_http_request string has to be freed
			free(raw_http_request);
			// no more possible leaks, since some fields may be initialized, is covered by the
			// freeHttpRequest implementation
			free_http_request(request);
			return NULL;
		}
		char* all = (char*)malloc_with_memset(resulting_index - currently_at + 1, true);

		if(!all) {
			return NULL;
		}

		// return pointer == all, so is ignored
		memcpy(all, currently_at, resulting_index - currently_at);

		char* method = NULL;
		char* path = NULL;
		char* protocol_version = NULL;

		// other way of checking if at the beginning
		if(currently_at == raw_http_request) {
			// parsing the string and inserting"\0" bytes at the" " space byte, so the three part
			// string can be used in three different fields, with the correct start address, this
			// trick is used more often trough-out this implementation, you don't have to understand
			// it, since its abstracted away when using only the provided function
			char* begin = strchr(all, ' ');
			if(begin == NULL) {
				// missing " " after the path
				return NULL;
			}
			*begin = '\0';
			method = all;
			all = begin + 1;
			begin = strchr(all, ' ');
			*begin = '\0';
			path = all;
			all = begin + 1;
			// is already null terminated!
			protocol_version = all;

			request->head.request_line = get_request_line_from_raw(method, path, protocol_version);

			free(method);

		} else {
			if(strlen(all) == 0) {
				// that denotes now comes the body! so the body is assigned and the loop ends with
				// the parsed = true the while loop finishes
				free(all);
				size_t body_length = strlen(raw_http_request) -
				                     ((resulting_index - raw_http_request) + separators_length);
				all = (char*)malloc_with_memset(body_length + 1, true);

				if(!all) {
					LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation,
					                   "Couldn't allocate memory!\n");
					return NULL;
				}

				memcpy(all, currently_at + separators_length, body_length);
				request->body = all;
				parsed = true;
			} else {
				// here headers are parsed, here":" is the delimiter

				// using same trick, the header string is one with the right 0 bytes :)
				char* begin = strchr(all, ':');
				*begin = '\0';
				if(*(begin + 1) == ' ') {
					++begin;
					*begin = '\0';
				}

				HttpHeaderField field = { .key = all, .value = begin + 1 };

				stbds_arrput(request->head.header_fields, field);
			}
		}

		// adjust the values
		currently_at = resulting_index + separators_length;

	} while(!parsed);

	// at the end free the input raw_http_request string
	free(raw_http_request);
	return request;
}

NODISCARD ParsedSearchPathEntry* find_search_key(ParsedSearchPath path, const char* key) {

	if(path.hash_map == STBDS_ARRAY_EMPTY) {
		// note: if hash_map is NULL stbds_shgeti allocates a new value, that is nevere populated to
		// the original ParsedSearchPath value, as this is a struct copy!
		return NULL;
	}

	int index = stbds_shgeti(path.hash_map, key);

	if(index < 0) {
		return NULL;
	}

	return &path.hash_map[index];
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

	for(size_t i = 0; i < stbds_arrlenu(array); ++i) {
		HttpHeaderField* header = &(array[i]);
		if(strcasecmp(header->key, key) == 0) {
			return header;
		}
	}

	return NULL;
}

static CompressionType parse_compression_type(char* compression_name, bool* ok_result) {
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

static CompressionValue parse_compression_value(char* compression_name, bool* ok_result) {

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
CompressionSettings* get_compression_settings(HttpHeaderFields header_fields) {

	CompressionSettings* compression_settings =
	    (CompressionSettings*)malloc(sizeof(CompressionSettings));

	if(!compression_settings) {
		return NULL;
	}

	compression_settings->entries = STBDS_ARRAY_EMPTY;

	// see: https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

	HttpHeaderField* accept_encoding_header = find_header_by_key(header_fields, "accept-encoding");

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

				float value = parse_compression_quality(compression_weight);

				if(!isnan(value)) {
					entry.weight = value;
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

				stbds_arrput(compression_settings->entries, entry);
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

static void free_compression_settings(CompressionSettings* compression_settings) {
	stbds_arrfree(compression_settings->entries);
	free(compression_settings);
}

RequestSettings* get_request_settings(HttpRequest* http_request) {

	RequestSettings* request_settings =
	    (RequestSettings*)malloc_with_memset(sizeof(RequestSettings), true);

	if(!request_settings) {
		return NULL;
	}

	CompressionSettings* compression_settings =
	    get_compression_settings(http_request->head.header_fields);

	if(!compression_settings) {
		free(request_settings);
		return NULL;
	}

	request_settings->compression_settings = compression_settings;

	return request_settings;
}

void free_request_settings(RequestSettings* request_settings) {

	free_compression_settings(request_settings->compression_settings);
	free(request_settings);
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
		if(is_compressions_supported(compression)) {
			return compression;
		}
	}

	return CompressionTypeNone;
}

static int compare_function_entries(
    const ANY_TYPE(CompressionEntry) // NOLINT(bugprone-easily-swappable-parameters)
    entry1_ign,
    const ANY_TYPE(CompressionEntry) entry2_ign) {
	const CompressionEntry* entry1 = (CompressionEntry*)entry1_ign;
	const CompressionEntry* entry2 = (CompressionEntry*)entry2_ign;

	// note weight is between 0.0 and 1.0

	if(entry1->weight != entry2->weight) {
		return (
		    int)((entry1->weight - entry2->weight) *
		         10000.0F); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	return 0;
}

SendSettings get_send_settings(RequestSettings* request_settings) {

	SendSettings result = { .compression_to_use = CompressionTypeNone };

	CompressionEntries entries = request_settings->compression_settings->entries;

	size_t entries_length = stbds_arrlenu(entries);

	if(entries_length == 0) {
		return result;
	}

	// this sorts the entries by weight, same weight means, we prefer the ones that come first
	// in the string, as it is unspecified in the spec, on what to sort as 2. criterium
	qsort(entries, entries_length, sizeof(CompressionEntry), compare_function_entries);

	for(size_t i = 0; i < entries_length; ++i) {
		CompressionEntry entry = entries[i];

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
				if(is_compressions_supported(entry.value.data.normal_compression)) {
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

// makes a string_builder + a sized body from the HttpResponse, just does the opposite of parsing
// a Request, but with some slight modification
HttpConcattedResponse* http_response_concat(HttpResponse* response) {
	HttpConcattedResponse* concatted_response =
	    (HttpConcattedResponse*)malloc_with_memset(sizeof(HttpConcattedResponse), true);

	if(!concatted_response) {
		return NULL;
	}

	StringBuilder* result = string_builder_init();
	const char* const separators = "\r\n";

	STRING_BUILDER_APPENDF(result, return NULL;
	                       , "%s %s %s%s", response->head.response_line.protocol_version,
	                       response->head.response_line.status_code,
	                       response->head.response_line.status_message, separators);

	for(size_t i = 0; i < stbds_arrlenu(response->head.header_fields); ++i) {
		// same elegant freeing but two at once :)
		STRING_BUILDER_APPENDF(result, return NULL;
		                       , "%s: %s%s", response->head.header_fields[i].key,
		                       response->head.header_fields[i].value, separators);
	}

	string_builder_append_single(result, separators);

	concatted_response->headers = result;
	concatted_response->body = response->body;

	return concatted_response;
}

// free the HttpResponse, just freeing everything necessary
void free_http_response(HttpResponse* response) {
	// elegantly freeing three at once :)
	free(response->head.response_line.protocol_version);
	for(size_t i = 0; i < stbds_arrlenu(response->head.header_fields); ++i) {
		// same elegant freeing but two at once :)

		free(response->head.header_fields[i].key);
	}
	stbds_arrfree(response->head.header_fields);

	free_sized_buffer(response->body);

	free(response);
}

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is
// static, but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)

NODISCARD static StringBuilder*
html_from_string(StringBuilder* head_content, // NOLINT(bugprone-easily-swappable-parameters)
                 StringBuilder* script_content, StringBuilder* style_content,
                 StringBuilder* body_content) {

	StringBuilder* result = string_builder_init();

	string_builder_append_single(result, "<!DOCTYPE html><html>");
	string_builder_append_single(result, "<head>");
	string_builder_append_single(result, "<meta charset=\"UTF-8\">");
	string_builder_append_single(
	    result, "<meta name=\"description\" content=\"HTML generated by simple C Http Server\">");
	string_builder_append_single(result, "<meta name=\"author\" content=\"Totto16\">");
	string_builder_append_single(result, "<title>Page by Simple C Http Server</title>");
	if(head_content != NULL) {
		string_builder_append_string_builder(result, &head_content);
	}
	if(script_content != NULL) {
		string_builder_append_single(result, "<script type=\"text/javascript\">");

		string_builder_append_string_builder(result, &script_content);
		string_builder_append_single(result, "</script>");
		string_builder_append_single(
		    result,
		    "<noscript> Diese Seite Benötigt Javascript um zu funktionieren :( </noscript>");
	}
	if(style_content != NULL) {
		string_builder_append_single(result, "<style type=\"text/css\">");
		string_builder_append_string_builder(result, &style_content);
		string_builder_append_single(result, "</style>");
	}
	string_builder_append_single(result, "</head>");
	string_builder_append_single(result, "<body>");
	if(body_content != NULL) {
		string_builder_append_string_builder(result, &body_content);
	}
	string_builder_append_single(result, "</body>");
	string_builder_append_single(result, "</html>");

	return result;
}

StringBuilder* http_request_to_json(const HttpRequest* const request, bool https,
                                    SendSettings send_settings) {
	StringBuilder* body = string_builder_init();

	const char* method = get_http_method_string(request->head.request_line.method);
	char* path = get_http_url_path_string(request->head.request_line.path);
	const char* protocol_version =
	    get_http_protocol_version_string(request->head.request_line.protocol_version);

	string_builder_append_single(body, "{\"request\":\"");
	string_builder_append_single(body, method);
	string_builder_append_single(body, "\",");

	STRING_BUILDER_APPENDF(body, return NULL;, "\"path\": \"%s\",", path);
	free(path);

	string_builder_append_single(body, "\"protocol_version\":\"");
	string_builder_append_single(body, protocol_version);
	string_builder_append_single(body, "\",");

	STRING_BUILDER_APPENDF(body, return NULL;, "\"secure\":%s,", https ? "true" : "false");
	string_builder_append_single(body, "\"headers\":[");

	const size_t header_amount = stbds_arrlenu(request->head.header_fields);

	for(size_t i = 0; i < header_amount; ++i) {
		// same elegant freeing but wo at once :)
		STRING_BUILDER_APPENDF(body, return NULL;, "{\"header\":\"%s\", \"key\":\"%s\"}",
		                                         request->head.header_fields[i].key,
		                                         request->head.header_fields[i].value);
		if(i + 1 < header_amount) {
			string_builder_append_single(body, ", ");
		} else {
			string_builder_append_single(body, "],");
		}
	}
	STRING_BUILDER_APPENDF(body, return NULL;, "\"body\":\"%s\"", request->body);

	string_builder_append_single(body, ", \"settings\": {");

	STRING_BUILDER_APPENDF(body, return NULL;
	                       , "\"send_settings\":{\"compression\" : \"%s\"} }",
	                       get_string_for_compress_format(send_settings.compression_to_use));

	string_builder_append_single(body, "}");
	return body;
}

StringBuilder* http_request_to_html(const HttpRequest* const request, bool https,
                                    SendSettings send_settings) {
	StringBuilder* body = string_builder_init();

	const char* method = get_http_method_string(request->head.request_line.method);
	char* path = get_http_url_path_string(request->head.request_line.path);
	const char* protocol_version =
	    get_http_protocol_version_string(request->head.request_line.protocol_version);

	string_builder_append_single(body, "<h1 id=\"title\">HttpRequest:</h1><br>");

	string_builder_append_single(body, "<div id=\"request\"><div>Method:");
	string_builder_append_single(body, method);
	string_builder_append_single(body, "</div>");

	STRING_BUILDER_APPENDF(body, return NULL;, "<div>Path: %s</div>", path);
	free(path);

	string_builder_append_single(body, "<div>ProtocolVersion:");
	string_builder_append_single(body, protocol_version);
	string_builder_append_single(body, "</div>");
	STRING_BUILDER_APPENDF(
	    body, return NULL;,
	                      "<div>Secure : %s</div><button id=\"shutdown\"> Shutdown </button></div>",
	                      https ? "true" : "false");
	string_builder_append_single(body, "<div id=\"header\">");
	for(size_t i = 0; i < stbds_arrlenu(request->head.header_fields); ++i) {
		// same elegant freeing but wo at once :)
		STRING_BUILDER_APPENDF(
		    body, return NULL;
		    , "<div><h2>Header:</h2><br><h3>Key:</h3> %s<br><h3>Value:</h3> %s</div>",
		    request->head.header_fields[i].key, request->head.header_fields[i].value);
	}

	string_builder_append_single(body, "</div> <div id=\"settings\">");
	string_builder_append_single(body, "<h1>Settings:</h1> <br>");
	{
		string_builder_append_single(body, "</div> <div id=\"send_settings\">");
		string_builder_append_single(body, "<h2>Send Settings:</h2> <br>");
		STRING_BUILDER_APPENDF(body, return NULL;
		                       , "<h3>Compression:</h3> %s",
		                       get_string_for_compress_format(send_settings.compression_to_use));
		string_builder_append_single(body, "</div>");
	}
	string_builder_append_single(body, "</div>");

	string_builder_append_single(body, "</div> <div id=\"body\">");
	STRING_BUILDER_APPENDF(body, return NULL;, "<h1>Body:</h1> <br>%s", request->body);
	string_builder_append_single(body, "</div>");

	// style

	StringBuilder* style = string_builder_init();
	string_builder_append_single(
	    style, "body{background: linear-gradient( 90deg, rgb(255, 0, 0) 0%, rgb(255, 154, 0) 10%, "
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
	           "#send_settings {display:flex; flex-direction: column;align-items: "
	           "center;overflow-wrap: "
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

	StringBuilder* html_result = html_from_string(NULL, script, style, body);
	return html_result;
}
