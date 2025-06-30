

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/utils.h"

// /usr/bin/clang-20 -Dlzws_EXPORTS -D__RELATIVE_FILE_PATH__=\"src/utils.c\"  -pedantic -Wall -Wextra -Wconversion -pipe -std=gnu18 -O0 -g -flto=thin -fPIC -DLZWS_EXPORT_LIBRARY_FLAG -MD -MT src/CMakeFiles/lzws.dir/utils.c.o -MF CMakeFiles/lzws.dir/utils.c.o.d -o CMakeFiles/lzws.dir/utils.c.o -c /home/totto/Code/c_simple_http/subprojects/lzws-1.5.6/src/utils.c

// clang-20 -Isubprojects/lzws-1.5.6/liblzws.so.p -Isubprojects/lzws-1.5.6 -I../subprojects/lzws-1.5.6 -Isubprojects/lzws-1.5.6/src -I../subprojects/lzws-1.5.6/src -fdiagnostics-color=always -D_FILE_OFFSET_BITS=64 -Wall -Winvalid-pch -Wextra -Wpedantic -std=gnu17 -O0 -g -fPIC -MD -MQ subprojects/lzws-1.5.6/liblzws.so.p/src_utils.c.o -MF subprojects/lzws-1.5.6/liblzws.so.p/src_utils.c.o.d -o subprojects/lzws-1.5.6/liblzws.so.p/src_utils.c.o -c ../subprojects/lzws-1.5.6/src/utils.c



// see https://datatracker.ietf.org/doc/html/rfc7231#section-5.3.4

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	COMPRESSION_TYPE_NONE = 0,
	COMPRESSION_TYPE_GZIP,
	COMPRESSION_TYPE_DEFLATE,
	COMPRESSION_TYPE_BR,
	COMPRESSION_TYPE_ZSTD,
	COMPRESSION_TYPE_COMPRESS,
	COMPRESSION_TYPE_BZIP2
} COMPRESSION_TYPE;

NODISCARD bool is_compressions_supported(COMPRESSION_TYPE format);

NODISCARD const char* get_string_for_compress_format(COMPRESSION_TYPE format);

NODISCARD SizedBuffer compress_buffer_with(SizedBuffer buffer, COMPRESSION_TYPE format);

#ifdef __cplusplus
}
#endif
