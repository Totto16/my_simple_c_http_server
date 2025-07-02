

#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

typedef struct {
	SizedBuffer body;
	bool send_body_data;
} HTTPResponseBody;

typedef STBDS_ARRAY(HttpHeaderField) HttpHeaderFields;

typedef struct {
	HttpStatusCode status;
	HTTPResponseBody body;
	const char* mime_type;
	HttpHeaderFields additional_headers;
} HTTPResponseToSend;

NODISCARD int send_http_message_to_connection(const ConnectionDescriptor* descriptor,
                                              HTTPResponseToSend to_send,
                                              SendSettings send_settings);

NODISCARD int send_http_message_to_connection_advanced(const ConnectionDescriptor* descriptor,
                                                       HTTPResponseToSend to_send,
                                                       SendSettings send_settings,
                                                       HttpRequestHead request_head);

NODISCARD HTTPResponseBody http_response_body_from_static_string(const char* static_string);

NODISCARD HTTPResponseBody http_response_body_from_string(char* string);

NODISCARD HTTPResponseBody http_response_body_from_string_builder(StringBuilder** string_builder);

NODISCARD HTTPResponseBody http_response_body_from_data(void* data, size_t size);

NODISCARD HTTPResponseBody http_response_body_empty(void);
