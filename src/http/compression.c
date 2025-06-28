
#include "./compression.h"

COMPRESSION_TYPE_MASK get_supported_compressions(void) {

	COMPRESSION_TYPE_MASK supported_compressions = COMPRESSION_TYPE_MASK_NONE;

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
	supported_compressions = supported_compressions | COMPRESSION_TYPE_MASK_GZIP;
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
	supported_compressions = supported_compressions | COMPRESSION_TYPE_MASK_DEFLATE;
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
	supported_compressions = supported_compressions | COMPRESSION_TYPE_MASK_BR;
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD
	supported_compressions = supported_compressions | COMPRESSION_TYPE_MASK_ZSTD;
#endif

	return supported_compressions;
}

NODISCARD const char* get_string_for_compress_format(COMPRESSION_TYPE format) {

	switch(format) {
		case COMPRESSION_TYPE_NONE: return "none";
		case COMPRESSION_TYPE_GZIP: return "gzip";
		case COMPRESSION_TYPE_DEFLATE: return "deflate";
		case COMPRESSION_TYPE_BR: return "br";
		case COMPRESSION_TYPE_ZSTD: return "zstd";
		default: return "<unknown>";
	}
}

NODISCARD char* compress_string_with(char* string, COMPRESSION_TYPE format) {

	UNUSED(string);
	UNUSED(format);

	return NULL;
}
