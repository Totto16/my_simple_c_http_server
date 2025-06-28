

#pragma once

#include "utils/utils.h"

// see https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	COMPRESSION_TYPE_NONE = 0,
	COMPRESSION_TYPE_GZIP,
	COMPRESSION_TYPE_DEFLATE,
	COMPRESSION_TYPE_BR,
	COMPRESSION_TYPE_ZSTD
} COMPRESSION_TYPE;

NODISCARD bool is_compressions_supported(COMPRESSION_TYPE format);

NODISCARD const char* get_string_for_compress_format(COMPRESSION_TYPE format);

NODISCARD char* compress_string_with(char* string, COMPRESSION_TYPE format);
