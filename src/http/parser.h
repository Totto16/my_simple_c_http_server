

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "./protocol.h"
#include "./v2.h"

NODISCARD HTTPRequestMethod get_http_method_from_string(const char* method,
                                                        OUT_PARAM(bool) success);

typedef struct HTTPReaderImpl HTTPReader;

NODISCARD HTTPReader* NULLABLE
initialize_http_reader_from_connection(ConnectionDescriptor* descriptor);

typedef struct HTTPGeneralContextImpl HTTPGeneralContext;

NODISCARD HTTPGeneralContext* http_reader_get_general_context(HTTPReader* reader);

NODISCARD BufferedReader* http_reader_release_buffered_reader(HTTPReader* reader);

NODISCARD HTTP2Context* http_general_context_get_http2_context(HTTPGeneralContext* general_context);

NODISCARD HttpRequestResult get_http_request(HTTPReader* reader);

NODISCARD bool http_reader_more_available(const HTTPReader* reader);

NODISCARD bool finish_reader(HTTPReader* reader, ConnectionContext* context);

NODISCARD CompressionSettings get_compression_settings(HttpHeaderFields header_fields);

void free_compression_settings(CompressionSettings compression_settings);

void free_request_settings(RequestSettings request_settings);

NODISCARD RequestSettings get_request_settings(HttpRequest http_request);

#ifdef __cplusplus
}
#endif
