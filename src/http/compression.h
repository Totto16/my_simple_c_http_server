

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/sized_buffer.h"
#include "utils/utils.h"

// see https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	CompressionTypeNone = 0,
	CompressionTypeGzip,
	CompressionTypeDeflate,
	CompressionTypeBr,
	CompressionTypeZstd,
	CompressionTypeCompress
} CompressionType;

NODISCARD bool is_compressions_supported(CompressionType format);

NODISCARD const char* get_string_for_compress_format(CompressionType format);

NODISCARD SizedBuffer compress_buffer_with(SizedBuffer buffer, CompressionType format);

#ifdef __cplusplus
}
#endif
