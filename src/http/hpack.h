

#pragma once

#include "utils/sized_buffer.h"
#include "utils/utils.h"

NODISCARD SizedBuffer http2_hpack_decompress_data(SizedBuffer input);
