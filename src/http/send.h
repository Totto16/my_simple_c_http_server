

#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

typedef struct {
	SizedBuffer body;
	bool sendBodyData;
} HTTPResponseBody;

typedef STBDS_ARRAY(HttpHeaderField) HttpHeaderFields;

typedef struct {
	HttpStatusCode status;
	HTTPResponseBody body;
	const char* mime_type;
	HttpHeaderFields additional_headers;
} HTTPResponseToSend;

NODISCARD int sendHTTPMessageToConnection(const ConnectionDescriptor* descriptor,
                                          HTTPResponseToSend to_send, SendSettings send_settings);

NODISCARD int sendHTTPMessageToConnectionAdvanced(const ConnectionDescriptor* descriptor,
                                                  HTTPResponseToSend to_send,
                                                  SendSettings send_settings,
                                                  HttpRequestHead request_head);

NODISCARD HTTPResponseBody httpResponseBodyFromStaticString(const char* static_string);

NODISCARD HTTPResponseBody httpResponseBodyFromString(char* string);

NODISCARD HTTPResponseBody httpResponseBodyFromStringBuilder(StringBuilder** string_builder);

NODISCARD HTTPResponseBody httpResponseBodyFromData(void* data, size_t size);

NODISCARD HTTPResponseBody httpResponseBodyEmpty(void);
