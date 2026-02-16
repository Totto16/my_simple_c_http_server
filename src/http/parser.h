

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "./protocol.h"

NODISCARD HTTPRequestMethod get_http_method_from_string(const char* method,
                                                        OUT_PARAM(bool) success);

typedef struct HTTPReaderImpl HTTPReader;

NODISCARD HTTPReader* NULLABLE
initialize_http_reader_from_connection(ConnectionDescriptor* descriptor);

NODISCARD HttpRequestResult get_http_request(HTTPReader* reader);

NODISCARD bool http_reader_more_available(const HTTPReader* reader);

NODISCARD bool finish_reader(HTTPReader* reader, ConnectionContext* context);

NODISCARD CompressionSettings get_compression_settings(HttpHeaderFields header_fields);

void free_compression_settings(CompressionSettings compression_settings);

void free_request_settings(RequestSettings request_settings);

#ifdef __cplusplus
}
#endif
