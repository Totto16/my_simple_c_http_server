
#include "./compression.h"

#include "utils/log.h"

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
#include <zlib.h>
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

#define SIZED_BUFFER_ERROR (SizedBuffer){ .data = NULL, .size = 0 }

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
#define Z_WINDOW_SIZE 15       // 9-15
#define Z_MEMORY_USAGE_LEVEL 8 // 1-9

#define Z_GZIP_ENCODING 16 // not inside the zlib header, unfortunately

NODISCARD static SizedBuffer compress_buffer_with_gzip(SizedBuffer buffer) {

	size_t chunk_size = 1 << Z_WINDOW_SIZE;

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer resultBuffer = { .data = start_chunk, .size = 0 };

	z_stream zs = {};
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;

	zs.avail_in = buffer.size;
	zs.next_in = (Bytef*)buffer.data;
	zs.avail_out = chunk_size;
	zs.next_out = (Bytef*)resultBuffer.data;

	int result =
	    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, Z_WINDOW_SIZE | Z_GZIP_ENCODING,
	                 Z_MEMORY_USAGE_LEVEL, Z_DEFAULT_STRATEGY);

	if(result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in gzip compression initiliaization occured: %s\n",
		            zError(result));
		freeSizedBuffer(resultBuffer);

		return SIZED_BUFFER_ERROR;
	}

	while(true) {
		int deflateResult = deflate(&zs, Z_FINISH);

		if(deflateResult == Z_STREAM_END) {
			resultBuffer.size += (chunk_size - zs.avail_out);
			break;
		}

		if(deflateResult == Z_OK || deflateResult == Z_BUF_ERROR) {
			resultBuffer.size += (chunk_size - zs.avail_out);
			void* new_chunk = realloc(resultBuffer.data, resultBuffer.size + chunk_size);
			zs.avail_out = chunk_size;
			zs.next_out = (Bytef*)new_chunk + resultBuffer.size;
			resultBuffer.data = new_chunk;
			continue;
		}

		LOG_MESSAGE(LogLevelError, "An error in gzip compression processing occured: %s\n",
		            zError(deflateResult));
		free(resultBuffer.data);

		return SIZED_BUFFER_ERROR;
	}

	int deflateEndResult = deflateEnd(&zs);

	if(deflateEndResult != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in gzip compression stream end occured: %s\n",
		            zError(deflateEndResult));
		freeSizedBuffer(resultBuffer);

		return SIZED_BUFFER_ERROR;
	}

	return resultBuffer;
}
#endif

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

NODISCARD SizedBuffer compress_buffer_with(SizedBuffer buffer, COMPRESSION_TYPE format) {

	switch(format) {
		case COMPRESSION_TYPE_NONE: return SIZED_BUFFER_ERROR; ;
		case COMPRESSION_TYPE_GZIP: {

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
			return compress_buffer_with_gzip(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case COMPRESSION_TYPE_DEFLATE: {
		};
		case COMPRESSION_TYPE_BR: {
		};
		case COMPRESSION_TYPE_ZSTD: {
		};
		default: {
			return SIZED_BUFFER_ERROR;
		}
	}
}
