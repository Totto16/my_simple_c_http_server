
#include "./compression.h"

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
#error "TODO"
#endif
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
#error "TODO"
#endif
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
#error "TODO"
#endif
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD
#error "TODO"
#endif

bool is_compressions_supported(COMPRESSION_TYPE format) {

	switch(format) {
		case COMPRESSION_TYPE_NONE: {
			return true;
		}
		case COMPRESSION_TYPE_GZIP: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
			return true;
#else
			return false;
#endif
		}
		case COMPRESSION_TYPE_DEFLATE: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
			return true;
#else
			return false;
#endif
		}
		case COMPRESSION_TYPE_BR: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
			return true;
#else
			return false;
#endif
		}
		case COMPRESSION_TYPE_ZSTD: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD
			return true;
#else
			return false;
#endif
		}
		default: return false;
	}
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
