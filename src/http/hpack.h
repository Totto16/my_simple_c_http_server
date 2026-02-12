

#pragma once

#include "utils/sized_buffer.h"
#include "utils/utils.h"
#include "http/protocol.h"

NODISCARD HttpHeaderFields http2_hpack_decompress_data(SizedBuffer input);

void global_initialize_http2_hpack_data(void);

void global_free_http2_hpack_data(void);
