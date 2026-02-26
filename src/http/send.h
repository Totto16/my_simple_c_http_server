

#pragma once

#include "./parser.h"
#include "generic/secure.h"
#include "http/protocol.h"

typedef struct {
	SizedBuffer content;
	bool send_body_data;
} HTTPResponseBody;

typedef struct {
	HttpStatusCode status;
	HTTPResponseBody body;
	tstr mime_type;
	HttpHeaderFields additional_headers;
} HTTPResponseToSend;

NODISCARD int send_http_message_to_connection(HTTPGeneralContext* general_context,
                                              const ConnectionDescriptor* descriptor,
                                              HTTPResponseToSend to_send,
                                              SendSettings send_settings);

NODISCARD HTTPResponseBody http_response_body_from_static_string(const char* static_string,
                                                                 bool send_body);

NODISCARD HTTPResponseBody http_response_body_from_string(char* string, bool send_body);

NODISCARD HTTPResponseBody http_response_body_from_string_builder(StringBuilder** string_builder,
                                                                  bool send_body);

NODISCARD HTTPResponseBody http_response_body_from_data(void* data, size_t size, bool send_body);

NODISCARD HTTPResponseBody http_response_body_empty(void);

void global_setup_port_data(uint16_t port);
