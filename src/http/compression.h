

#pragma once

#include "utils/utils.h"

// see https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	COMPRESSION_TYPE_MASK_NONE = 0x00,
	COMPRESSION_TYPE_MASK_GZIP = 0x01,
	COMPRESSION_TYPE_MASK_DEFLATE = 0x02,
	COMPRESSION_TYPE_MASK_BR = 0x04,
	COMPRESSION_TYPE_MASK_ZSTD = 0x08,
	// deprecated, but still recognized
	COMPRESSION_TYPE_MASK_COMPRESS = 0x10,
} COMPRESSION_TYPE_MASK;

COMPRESSION_TYPE_MASK get_supported_compressions(void);

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

NODISCARD const char* get_string_for_compress_format(COMPRESSION_TYPE format);

NODISCARD char* compress_string_with(char* string, COMPRESSION_TYPE format);
