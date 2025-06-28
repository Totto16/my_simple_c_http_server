
#include "http_protocol.h"

// frees the HttpRequest, taking care of Null Pointer, this si needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void freeHttpRequest(HttpRequest* request) {
	freeIfNotNULL(request->head.requestLine.method);
	for(size_t i = 0; i < stbds_arrlenu(request->head.headerFields); ++i) {
		// same elegant freeing but two at once :)
		freeIfNotNULL(request->head.headerFields[i].key);
	}
	stbds_arrfree(request->head.headerFields);
	freeIfNotNULL(request->body);
	freeIfNotNULL(request);
}

// returning a stringbuilder, that makes a string from the httpRequest, this is useful for debugging
StringBuilder* httpRequestToStringBuilder(HttpRequest* request, bool https) {
	StringBuilder* result = string_builder_init();
	string_builder_append_single(result, "HttpRequest:\n");
	string_builder_append(result, return NULL;, "\tMethod: %s\n", request->head.requestLine.method);
	string_builder_append(result, return NULL;, "\tURI: %s\n", request->head.requestLine.URI);
	string_builder_append(result, return NULL;
	                      , "\tProtocolVersion : %s\n", request->head.requestLine.protocolVersion);

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
			// string can vbe used in three different fields, with the correct start address, this
			// trick is used more often trough-out this implementation, you don't have to understand
			// it, since its abstracted away when using only the provided function
			char* begin = index(all, ' ');
			*begin = '\0';
			request->head.requestLine.method = all;
			all = begin + 1;
			begin = index(all, ' ');
			*begin = '\0';
			request->head.requestLine.URI = all;
			all = begin + 1;
			// is already null terminated!
			request->head.requestLine.protocolVersion = all;

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
const char* getStatusMessage(int statusCode) {
	const char* result = "NOT SUPPORTED STATUS CODE";
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

static bool constructHeadersForRequest(size_t bodyLength, HttpHeaderField* additionalHeaders,
                                       size_t headersSize, const char* MIMEType,
                                       HttpResponse* response) {

	STBDS_ARRAY_INIT(response->head.headerFields);

	// add standard fields

	{
		// MIME TYPE

		if(MIMEType) {
			// add the standard ones, using %c with '\0' to use the trick, described above
			char* contentTypeBuffer = NULL;
			formatString(&contentTypeBuffer, return NULL;
			             , "%s%c%s", "Content-Type", '\0',
			             MIMEType == NULL ? DEFAULT_MIME_TYPE : MIMEType);

			size_t current_array_index = stbds_arrlenu(response->head.headerFields);

			stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

			response->head.headerFields[current_array_index].key = contentTypeBuffer;
			response->head.headerFields[current_array_index].value =
			    contentTypeBuffer + strlen(contentTypeBuffer) + 1;
		}
	}

	{
		// CONTENT LENGTH

		char* contentLengthBuffer = NULL;
		formatString(&contentLengthBuffer, return NULL;
		             , "%s%c%ld", "Content-Length", '\0', bodyLength);

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		response->head.headerFields[current_array_index].key = contentLengthBuffer;
		response->head.headerFields[current_array_index].value =
		    contentLengthBuffer + strlen(contentLengthBuffer) + 1;
	}

	{
		// Server

		char* serverBuffer = NULL;
		formatString(&serverBuffer, return NULL;
		             , "%s%c%s", "Server", '\0',
		             "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING));

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		response->head.headerFields[current_array_index].key = serverBuffer;
		response->head.headerFields[current_array_index].value =
		    serverBuffer + strlen(serverBuffer) + 1;
	}

	size_t current_array_size = stbds_arrlenu(response->head.headerFields);

	stbds_arrsetcap(response->head.headerFields, current_array_size + headersSize);

	for(size_t i = 0; i < headersSize; ++i) {

		size_t current_array_index = stbds_arrlenu(response->head.headerFields);

		stbds_arrsetlen(response->head.headerFields, current_array_index + 1);

		// ATTENTION; this things have to be ALL malloced
		response->head.headerFields[current_array_index].key = additionalHeaders[i].key;
		response->head.headerFields[current_array_index].value = additionalHeaders[i].value;
	}

	// if additional Headers are specified free them now
	if(headersSize > 0) {
		free(additionalHeaders);
	}

	return true;
}

// simple http Response constructor using string builder, headers can be NULL, when headerSize is
// also null!
HttpResponse* constructHttpResponseWithHeaders(int status, char* body,
                                               HttpHeaderField* additionalHeaders,
                                               size_t headersSize, const char* MIMEType) {

	HttpResponse* response = (HttpResponse*)mallocWithMemset(sizeof(HttpResponse), true);

	if(!response) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	// using the same trick as before, \0 in the malloced string :)
	const char* protocolVersion = "HTTP/1.1";
	size_t protocolLength = strlen(protocolVersion);
	const char* statusMessage = getStatusMessage(status);

	char* responseLineBuffer = NULL;
	formatString(&responseLineBuffer, return NULL;
	             , "%s%c%d%c%s", protocolVersion, '\0', status, '\0', statusMessage);

	response->head.responseLine.protocolVersion = responseLineBuffer;
	response->head.responseLine.statusCode = responseLineBuffer + protocolLength + 1;
	response->head.responseLine.statusMessage =
	    responseLineBuffer + protocolLength + strlen(responseLineBuffer + protocolLength + 1) + 2;

	size_t bodyLength = body ? strlen(body) : 0;

	if(!constructHeadersForRequest(bodyLength, additionalHeaders, headersSize, MIMEType,
	                               response)) {
		// TODO(Totto): free things accordingly
		return NULL;
	}

	// for that the body has to be malloced
	response->body = body;
	// finally retuning the malloced httpResponse
	return response;
}

// wrapper if no additionalHeaders are required
HttpResponse* constructHttpResponse(int status, char* body, const char* MIMEType) {
	return constructHttpResponseWithHeaders(status, body, NULL, 0, MIMEType);
}

// makes a stringBuilder from the HttpResponse, just does the opposite of parsing A Request, but
// with some slight modification
StringBuilder* httpResponseToStringBuilder(HttpResponse* response) {
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

	if(response->body) {
		string_builder_append_single(result, response->body);
	}

	return result;
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

	if(response->body) {
		free(response->body);
	}

	free(response);
}

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is static,
// but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)

char* htmlFromString(char* headContent, char* scriptContent, char* styleContent,
                     char* bodyContent) {

	StringBuilder* result = string_builder_init();

	string_builder_append_single(result, "<!DOCTYPE html><html>");
	string_builder_append_single(result, "<head>");
	string_builder_append_single(result, "<meta charset=\"UTF-8\">");
	string_builder_append_single(
	    result, "<meta name=\"description\" content=\"HTML generated by simple C Http Server\">");
	string_builder_append_single(result, "<meta name=\"author\" content=\"Totto16\">");
	string_builder_append_single(result, "<title>Page by Simple C Http Server</title>");
	if(headContent != NULL) {
		string_builder_append(result, return NULL;, "%s", headContent);
	}
	if(scriptContent != NULL) {
		string_builder_append(result, return NULL;
		                      , "<script type=\"text/javascript\">%s</script>", scriptContent);
		string_builder_append_single(
		    result,
		    "<noscript> Diese Seite Benötigt Javascript um zu funktionieren :( </noscript>");
	}
	if(styleContent != NULL) {
		string_builder_append(result, return NULL;
		                      , "<style type=\"text/css\">%s</style>", styleContent);
	}
	string_builder_append_single(result, "</head>");
	string_builder_append_single(result, "<body>");
	if(bodyContent != NULL) {
		string_builder_append(result, return NULL;, "%s", bodyContent);
	}
	string_builder_append_single(result, "</body>");
	string_builder_append_single(result, "</html>");

	return string_builder_to_string(result);
}

char* httpRequestToJSON(HttpRequest* request, bool https) {
	StringBuilder* body = string_builder_init();
	string_builder_append(body, return NULL;
	                      , "{\"request\":\"%s\",", request->head.requestLine.method);
	string_builder_append(body, return NULL;, "\"URI\": \"%s\",", request->head.requestLine.URI);
	string_builder_append(body, return NULL;
	                      , "\"version\":\"%s\",", request->head.requestLine.protocolVersion);
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
	string_builder_append(body, return NULL;, "\"body\":\"%s\"}", request->body);

	return string_builder_to_string(body);
}

char* httpRequestToHtml(HttpRequest* request, bool https) {
	StringBuilder* body = string_builder_init();
	string_builder_append_single(body, "<h1 id=\"title\">HttpRequest:</h1><br>");
	string_builder_append(body, return NULL;, "<div id=\"request\"><div>Method: %s</div>",
	                                        request->head.requestLine.method);
	string_builder_append(body, return NULL;, "<div>URI: %s</div>", request->head.requestLine.URI);
	string_builder_append(body, return NULL;, "<div>ProtocolVersion : %s</div>",
	                                        request->head.requestLine.protocolVersion);
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
	string_builder_append_single(body, "</div> <div id=\"body\">");
	string_builder_append(body, return NULL;, "<h1>Body:</h1> <br>%s", request->body);
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
	           "#title{text-align: center;}");

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

	char* htmlResult =
	    htmlFromString(NULL, string_builder_get_string(script), string_builder_get_string(style),
	                   string_builder_get_string(body));
	string_builder_free(body);
	string_builder_free(style);
	string_builder_free(script);
	return htmlResult;
}
