

#pragma once

#include "./protocol.h"

typedef struct HTTPReaderImpl HTTPReader;

NODISCARD HTTPReader* NULLABLE
initialize_http_reader_from_connection(ConnectionDescriptor* descriptor);

NODISCARD HttpRequestResult get_http_request(HTTPReader* reader);

NODISCARD bool http_reader_more_available(const HTTPReader* reader);

NODISCARD bool finish_reader(HTTPReader* reader, ConnectionContext* context);
