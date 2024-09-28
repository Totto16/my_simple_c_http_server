
#include "http_protocol.h"

// frees the HttpRequest, taking care of Null Pointer, this si needed for some corrupted requests,
// when a corrupted request e.g was parsed partly correct
void freeHttpRequest(HttpRequest* request) {
	freeIfNotNULL(request->head.requestLine.method);
	for(size_t i = 0; i < request->head.headerAmount; ++i) {
		// same elegant freeing but two at once :)
		freeIfNotNULL(request->head.headerFields[i].key);
	}
	freeIfNotNULL(request->head.headerFields);
	freeIfNotNULL(request->body);
	freeIfNotNULL(request);
}

// returning a stringbuilder, that makes a string from the httpRequest, this is useful for debugging
StringBuilder* httpRequestToStringBuilder(HttpRequest* request) {
	StringBuilder* result = string_builder_init();
	string_builder_append_single(result, "HttpRequest:\n");
	string_builder_append(result, "\tMethod: %s\n", request->head.requestLine.method);
	string_builder_append(result, "\tURI: %s\n", request->head.requestLine.URI);
	string_builder_append(result, "\tProtocolVersion : %s\n",
	                      request->head.requestLine.protocolVersion);

	for(size_t i = 0; i < request->head.headerAmount; ++i) {
		// same elegant freeing but wo at once :)
		string_builder_append(result, "\tHeader:\n\t\tKey: %s \n\t\tValue: %s\n",
		                      request->head.headerFields[i].key,
		                      request->head.headerFields[i].value);
	}
	string_builder_append(result, "\tBody: %s\n", request->body);
	return result;
}

// if the parsing did go wrong NULL is returned otherwise everything is filled with malloced
// strings, but keep in mind that you gave to use the given free method to free that properly,
// internally some string"magic" happens
HttpRequest* parseHttpRequest(char* rawHttpRequest) {

	// considered using strtok, but that doesn't recognize the delimiter between the status and
	// body! so now using own way of doing that!

	char const* separators = "\r\n";
	size_t separatorsLength = strlen(separators);
	char* currentlyAt = rawHttpRequest;
	bool parsed = false;
	HttpRequest* request = (HttpRequest*)mallocOrFail(sizeof(HttpRequest), true);
	// iterating over each separated string, then determining if header or body or statusLine and
	// then parsing that accordingly
	do {
		char* resultingIndex = strstr(currentlyAt, separators);
		// no"\r\n" could be found, so a parse Error occurred, a NULl signals that
		if(resultingIndex == NULL) {
			// also the input rawHttpRequest string has to be freed
			free(rawHttpRequest);
			// no more possible leaks, since some fields may be initialized, is covered by the
			// freeHttpRequest implementation
			freeHttpRequest(request);
			return NULL;
		}
		char* all = (char*)mallocOrFail(resultingIndex - currentlyAt + 1, true);
		// return pointer == all, so is ignored
		memcpy(all, currentlyAt, resultingIndex - currentlyAt);

		// other way of checking if at the beginning
		if(currentlyAt == rawHttpRequest) {
			// parsing the string and inserting"\0" bytes at the" " space byte, so the three part
			// string can vbe used in three different fields, with the correct start address, this
			// trick is used more often troughout this implementation, you don't have to understand
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
				// the parsed = true the while loop finshes
				free(all);
				size_t bodyLength =
				    strlen(rawHttpRequest) - ((resultingIndex - rawHttpRequest) + separatorsLength);
				all = (char*)mallocOrFail(bodyLength + 1, true);
				memcpy(all, currentlyAt + separatorsLength, bodyLength);
				request->body = all;
				parsed = true;
			} else {
				// here headers are parsed, here":" is the delimiter
				if(request->head.headerAmount == 0) {
					request->head.headerFields =
					    (HttpHeaderField*)mallocOrFail(sizeof(HttpHeaderField), true);
				} else {
					request->head.headerFields = (HttpHeaderField*)reallocOrFail(
					    request->head.headerFields,
					    sizeof(HttpHeaderField) * request->head.headerAmount,
					    sizeof(HttpHeaderField) * (request->head.headerAmount + 1), true);
				}

				// using same trick, the header string is one with the right 0 bytes :)
				char* begin = index(all, ':');
				*begin = '\0';
				if(*(begin + 1) == ' ') {
					++begin;
					*begin = '\0';
				}

				request->head.headerFields[request->head.headerAmount].key = all;
				request->head.headerFields[request->head.headerAmount].value = begin + 1;
				++request->head.headerAmount;
			}
		}

		// adjust the values
		currentlyAt = resultingIndex + separatorsLength;

	} while(!parsed);

	// at the end free the input rawHttpRequest string
	free(rawHttpRequest);
	return request;
}

// simple helper for getting the status Message for a special status code, all from teh spec for http 1.1 implemented (not in the spec e.g. 418)
char const* getStatusMessage(int statusCode) {
	char const* result = "NOT SUPPORTED STATUS CODE";
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
	}
	return result;
}

// simple http Response constructor using string builder, headers can be NULL, when headerSize is
// also null!
HttpResponse* constructHttpResponseWithHeaders(int status, char* body,
                                               HttpHeaderField* additionalHeaders,
                                               size_t headersSize, char const* MIMEType) {

	HttpResponse* response = (HttpResponse*)mallocOrFail(sizeof(HttpResponse), true);

	// using the same trick as before, \0 in the malloced string :)
	char const* protocolVersion = "HTTP/1.1";
	size_t protocolLength = strlen(protocolVersion);
	char const* statusMessage = getStatusMessage(status);

	char* responseLineBuffer = NULL;
	formatString(&responseLineBuffer, "%s%c%d%c%s", protocolVersion, '\0', status, '\0',
	             statusMessage);

	response->head.responseLine.protocolVersion = responseLineBuffer;
	response->head.responseLine.statusCode = responseLineBuffer + protocolLength + 1;
	response->head.responseLine.statusMessage =
	    responseLineBuffer + protocolLength + strlen(responseLineBuffer + protocolLength + 1) + 2;

	// now adding headers, adjust this value for more manual fields that are available in the array
	// you have to asssign below
#define STANDARD_HEADER_LENGTH 3

	for(size_t i = 0; i < STANDARD_HEADER_LENGTH + headersSize; ++i) {
		if(response->head.headerAmount == 0) {
			response->head.headerFields =
			    (HttpHeaderField*)mallocOrFail(sizeof(HttpHeaderField), true);
		} else {
			response->head.headerFields = (HttpHeaderField*)reallocOrFail(
			    response->head.headerFields, sizeof(HttpHeaderField) * response->head.headerAmount,
			    sizeof(HttpHeaderField) * (response->head.headerAmount + 1), true);
		}

		if(i >= STANDARD_HEADER_LENGTH) {
			// ATTENTION; this things have to be ALL malloced
			response->head.headerFields[response->head.headerAmount].key =
			    additionalHeaders[i - STANDARD_HEADER_LENGTH].key;
			response->head.headerFields[response->head.headerAmount].value =
			    additionalHeaders[i - STANDARD_HEADER_LENGTH].value;
		}
		++response->head.headerAmount;
	}

	// if additional Headers are specified free them now
	if(headersSize > 0) {
		free(additionalHeaders);
	}

	// add the standard ones, using %c with '\0' to use the trick, described above
	char* contentTypeBuffer = NULL;
	formatString(&contentTypeBuffer, "%s%c%s", "Content-Type", '\0',
	             MIMEType == NULL ? DEFAULT_MIME_TYPE : MIMEType);

	char* contentLengthBuffer = NULL;
	formatString(&contentLengthBuffer, "%s%c%ld", "Content-Length", '\0', strlen(body));

	char* serverBuffer = NULL;
	formatString(&serverBuffer, "%s%c%s", "Server", '\0',
	             "Simple C HTTP Server: v" STRINGIFY(VERSION_STRING));

	response->head.headerFields[0].key = contentTypeBuffer;
	response->head.headerFields[0].value = contentTypeBuffer + strlen(contentTypeBuffer) + 1;

	response->head.headerFields[1].key = contentLengthBuffer;
	response->head.headerFields[1].value = contentLengthBuffer + strlen(contentLengthBuffer) + 1;

	response->head.headerFields[2].key = serverBuffer;
	response->head.headerFields[2].value = serverBuffer + strlen(serverBuffer) + 1;

	// for that the body has to be malloced
	response->body = body;
	// finally retuning the malloced httpResponse
	return response;
}

// wrapper if no additionalHeaders are required
HttpResponse* constructHttpResponse(int status, char* body, char const* MIMEType) {
	return constructHttpResponseWithHeaders(status, body, NULL, 0, MIMEType);
}

// makes a stringBuilder from the HttpResponse, just does the opposite of parsing A Request, but
// with some slight modification
StringBuilder* httpResponseToStringBuilder(HttpResponse* response) {
	StringBuilder* result = string_builder_init();
	char const* separators = "\r\n";

	string_builder_append(result, "%s %s %s%s", response->head.responseLine.protocolVersion,
	                      response->head.responseLine.statusCode,
	                      response->head.responseLine.statusMessage, separators);

	for(size_t i = 0; i < response->head.headerAmount; ++i) {
		// same elegant freeing but two at once :)
		string_builder_append(result, "%s: %s%s", response->head.headerFields[i].key,
		                      response->head.headerFields[i].value, separators);
	}
	string_builder_append(result, "%s%s", separators, response->body);
	return result;
}

// free the HttpResponse, just freeing everything necessary
void freeHttpResponse(HttpResponse* response) {
	// elegantly freeing three at once :)
	free(response->head.responseLine.protocolVersion);
	for(size_t i = 0; i < response->head.headerAmount; ++i) {
		// same elegant freeing but two at once :)

		free(response->head.headerFields[i].key);
	}
	free(response->head.headerFields);
	free(response->body);
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
	string_builder_append_single(
	    result, "<meta name=\"author\" content=\"Tobias Niederbrunner - csba1761\">");
	string_builder_append_single(result, "<title> Page by Simple C Http Server</title>");
	if(headContent != NULL) {
		string_builder_append(result, "%s", headContent);
	}
	if(scriptContent != NULL) {
		string_builder_append(result, "<script type=\"text/javascript\">%s</script>",
		                      scriptContent);
		string_builder_append_single(
		    result,
		    "<noscript> Diese Seite Benötigt Javascript um zu funktionieren :( </noscript>");
	}
	if(styleContent != NULL) {
		string_builder_append(result, "<style type=\"text/css\">%s</style>", styleContent);
	}
	string_builder_append_single(result, "</head>");
	string_builder_append_single(result, "<body>");
	if(bodyContent != NULL) {
		string_builder_append(result, "%s", bodyContent);
	}
	string_builder_append_single(result, "</body>");
	string_builder_append_single(result, "</html>");

	return string_builder_to_string(result);
}

char* httpRequestToJSON(HttpRequest* request) {
	StringBuilder* body = string_builder_init();
	string_builder_append(body, "{\"request\":\"%s\",", request->head.requestLine.method);
	string_builder_append(body, "\"URI\": \"%s\",", request->head.requestLine.URI);
	string_builder_append(body, "\"version\":\"%s\",", request->head.requestLine.protocolVersion);
	string_builder_append_single(body, "\"headers\":[");
	for(size_t i = 0; i < request->head.headerAmount; ++i) {
		// same elegant freeing but wo at once :)
		string_builder_append(body, "{\"header\":\"%s\", \"key\":\"%s\"}",
		                      request->head.headerFields[i].key,
		                      request->head.headerFields[i].value);
		if(i + 1 < request->head.headerAmount) {
			string_builder_append_single(body, ", ");
		} else {
			string_builder_append_single(body, "],");
		}
	}
	string_builder_append(body, "\"body\":\"%s\"}", request->body);

	return string_builder_to_string(body);
}

char* httpRequestToHtml(HttpRequest* request) {
	StringBuilder* body = string_builder_init();
	string_builder_append_single(body, "<h1 id=\"title\">HttpRequest:</h1><br>");
	string_builder_append(body, "<div id=\"request\"><div>Method: %s</div>",
	                      request->head.requestLine.method);
	string_builder_append(body, "<div>URI: %s</div>", request->head.requestLine.URI);
	string_builder_append(
	    body, "<div>ProtocolVersion : %s</div><button id=\"shutdown\"> Shutdown </button></div>",
	    request->head.requestLine.protocolVersion);
	string_builder_append_single(body, "<div id=\"header\">");
	for(size_t i = 0; i < request->head.headerAmount; ++i) {
		// same elegant freeing but wo at once :)
		string_builder_append(
		    body, "<div><h2>Header:</h2><br><h3>Key:</h3> %s<br><h3>Value:</h3> %s</div>",
		    request->head.headerFields[i].key, request->head.headerFields[i].value);
	}
	string_builder_append_single(body, "</div> <div id=\"body\">");
	string_builder_append(body, "<h1>Body:</h1> <br>%s", request->body);
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
