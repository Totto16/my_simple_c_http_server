
#include "./compression.h"

#include "utils/log.h"

#if defined(_SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP) || \
    defined(_SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE)
#include <zlib.h>
#endif
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
#include <brotli/encode.h>
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

#if defined(_SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP) || \
    defined(_SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE)

#define Z_WINDOW_SIZE 15       // 9-15
#define Z_MEMORY_USAGE_LEVEL 8 // 1-9

#define Z_GZIP_ENCODING 16 // not inside the zlib header, unfortunately

NODISCARD static SizedBuffer compress_buffer_with_zlib(SizedBuffer buffer, bool gzip) {

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

	int windowBits = Z_WINDOW_SIZE;

	if(gzip) {
		windowBits = windowBits | Z_GZIP_ENCODING;
	}

	int result = deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits,
	                          Z_MEMORY_USAGE_LEVEL, Z_DEFAULT_STRATEGY);

	if(result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in gzip compression initiliaization occured: %s\n",
		            zError(result));
		freeSizedBuffer(resultBuffer);

		return SIZED_BUFFER_ERROR;
	}

	while(true) {
		int deflateResult = deflate(&zs, Z_FINISH);

		resultBuffer.size += (chunk_size - zs.avail_out);

		if(deflateResult == Z_STREAM_END) {
			break;
		}

		if(deflateResult == Z_OK || deflateResult == Z_BUF_ERROR) {
			void* new_chunk = realloc(resultBuffer.data, resultBuffer.size + chunk_size);
			resultBuffer.data = new_chunk;

			zs.avail_out = chunk_size;
			zs.next_out = (Bytef*)new_chunk + resultBuffer.size;
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

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
static SizedBuffer compress_buffer_with_gzip(SizedBuffer buffer) {
	return compress_buffer_with_zlib(buffer, true);
}
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
static SizedBuffer compress_buffer_with_deflate(SizedBuffer buffer) {
	return compress_buffer_with_zlib(buffer, false);
}
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR

#define BROTLI_QUALITY 11     // 0-11
#define BROTLI_WINDOW_SIZE 15 // 10-24

static SizedBuffer compress_buffer_with_br(SizedBuffer buffer) {

	BrotliEncoderState* state = BrotliEncoderCreateInstance(NULL, NULL, NULL);

	if(!state) {
		LOG_MESSAGE_SIMPLE(
		    LogLevelError,
		    "An error in brotli compression initiliaization occured: failed to initialize state");

		return SIZED_BUFFER_ERROR;
	}

	if(!BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, BROTLI_QUALITY)) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "An error in brotli compression initiliaization occured: "
		                                  "failed to set parameter quality");

		return SIZED_BUFFER_ERROR;
	};

	if(!BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, BROTLI_WINDOW_SIZE)) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "An error in brotli compression initiliaization occured: "
		                                  "failed to set parameter sliding window size");

		return SIZED_BUFFER_ERROR;
	}; // 0-11

	size_t chunk_size = (1 << BROTLI_WINDOW_SIZE) - 16;

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		BrotliEncoderDestroyInstance(state);
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer resultBuffer = { .data = start_chunk, .size = 0 };

	size_t available_in = buffer.size;

	const uint8_t* next_in = buffer.data;

	size_t available_out = chunk_size;

	uint8_t* next_out = resultBuffer.data;

	while(true) {

		BrotliEncoderOperation encoding_op =
		    available_in == 0 ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;

		size_t available_out_before = available_out;

		BROTLI_BOOL result = BrotliEncoderCompressStream(state, encoding_op, &available_in,
		                                                 &next_in, &available_out, &next_out, NULL);
		if(result == BROTLI_FALSE) {
			LOG_MESSAGE(LogLevelError, "An error in brotli compression processing occured: %s\n",
			            zError(result));
			freeSizedBuffer(resultBuffer);
			BrotliEncoderDestroyInstance(state);

			return SIZED_BUFFER_ERROR;
		}

		resultBuffer.size += (available_out_before - available_out);

		if(available_out == 0) {
			void* new_chunk = realloc(resultBuffer.data, resultBuffer.size + chunk_size);
			resultBuffer.data = new_chunk;

			available_out += chunk_size;
			next_out = (uint8_t*)new_chunk + resultBuffer.size;
		}

		if(available_in == 0) {
			break;
		}

		//
	}

	BrotliEncoderDestroyInstance(state);
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
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
			return compress_buffer_with_deflate(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case COMPRESSION_TYPE_BR: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
			return compress_buffer_with_br(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case COMPRESSION_TYPE_ZSTD: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD
			return compress_buffer_with_zstd(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		default: {
			return SIZED_BUFFER_ERROR;
		}
	}
}
