

#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

typedef struct {
	SizedBuffer body;
	bool sendBodyData;
} HTTPResponseBody;

typedef STBDS_ARRAY(HttpHeaderField) HttpHeaderFields;

typedef struct {
	HTTP_STATUS_CODES status;
	HTTPResponseBody body;
	const char* MIMEType;
	HttpHeaderFields additionalHeaders;
} HTTPResponseToSend;

NODISCARD int sendHTTPMessageToConnection(const ConnectionDescriptor* descriptor,
                                          HTTPResponseToSend toSend, SendSettings send_settings);

NODISCARD int sendHTTPMessageToConnectionAdvanced(const ConnectionDescriptor* descriptor,
                                                  HTTPResponseToSend toSend,
                                                  SendSettings send_settings,
                                                  HttpRequestHead request_head);

NODISCARD HTTPResponseBody httpResponseBodyFromStaticString(const char* static_string);

NODISCARD HTTPResponseBody httpResponseBodyFromString(char* string);

NODISCARD HTTPResponseBody httpResponseBodyFromStringBuilder(StringBuilder** stringBuilder);

NODISCARD HTTPResponseBody httpResponseBodyFromData(void* data, size_t size);

NODISCARD HTTPResponseBody httpResponseBodyEmpty(void);
