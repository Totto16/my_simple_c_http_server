
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
#include <zstd.h>
#endif
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_COMPRESS
#include <buffer.h>
#include <compressor/main.h>
#endif

bool is_compressions_supported(COMPRESSION_TYPE format) {

	switch(format) {
		case COMPRESSION_TYPE_NONE: { // NOLINT(bugprone-branch-clone)
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
		case COMPRESSION_TYPE_COMPRESS: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_COMPRESS
			return true;
#else
			return false;
#endif
		}
		default: return false;
	}
}

#define SIZED_BUFFER_ERROR get_empty_sized_buffer()

#if defined(_SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP) || \
    defined(_SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE)

#define Z_WINDOW_SIZE 15       // 9-15
#define Z_MEMORY_USAGE_LEVEL 8 // 1-9

#define Z_GZIP_ENCODING 16 // not inside the zlib header, unfortunately

NODISCARD static SizedBuffer compress_buffer_with_zlib(SizedBuffer buffer, bool gzip) {

	const size_t chunk_size = 1 << Z_WINDOW_SIZE;

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer resultBuffer = { .data = start_chunk, .size = 0 };

	z_stream zstream = {};
	zstream.zalloc = Z_NULL;
	zstream.zfree = Z_NULL;
	zstream.opaque = Z_NULL;

	zstream.avail_in = buffer.size;
	zstream.next_in = (Bytef*)buffer.data;
	zstream.avail_out = chunk_size;
	zstream.next_out = (Bytef*)resultBuffer.data;

	int windowBits = Z_WINDOW_SIZE;

	if(gzip) {
		windowBits = windowBits | Z_GZIP_ENCODING;
	}

	int result = deflateInit2(&zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits,
	                          Z_MEMORY_USAGE_LEVEL, Z_DEFAULT_STRATEGY);

	if(result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in gzip compression initiliaization occured: %s\n",
		            zError(result));
		freeSizedBuffer(resultBuffer);

		return SIZED_BUFFER_ERROR;
	}

	while(true) {
		int deflateResult = deflate(&zstream, Z_FINISH);

		resultBuffer.size += (chunk_size - zstream.avail_out);

		if(deflateResult == Z_STREAM_END) {
			break;
		}

		if(deflateResult == Z_OK || deflateResult == Z_BUF_ERROR) {
			void* new_chunk = realloc(resultBuffer.data, resultBuffer.size + chunk_size);
			resultBuffer.data = new_chunk;

			zstream.avail_out = chunk_size;
			zstream.next_out = (Bytef*)new_chunk + resultBuffer.size;
			continue;
		}

		LOG_MESSAGE(LogLevelError, "An error in gzip compression processing occured: %s\n",
		            zError(deflateResult));
		free(resultBuffer.data);

		return SIZED_BUFFER_ERROR;
	}

	int deflateEndResult = deflateEnd(&zstream);

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
		    "An error in brotli compression initiliaization occured: failed to initialize state\n");

		return SIZED_BUFFER_ERROR;
	}

	if(!BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, BROTLI_QUALITY)) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "An error in brotli compression initiliaization occured: "
		                                  "failed to set parameter quality\n");

		return SIZED_BUFFER_ERROR;
	};

	if(!BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, BROTLI_WINDOW_SIZE)) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "An error in brotli compression initiliaization occured: "
		                                  "failed to set parameter sliding window size\n");

		return SIZED_BUFFER_ERROR;
	}; // 0-11

	const size_t chunk_size = (1 << BROTLI_WINDOW_SIZE) - 16;

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

		const size_t available_out_before = available_out;

		BROTLI_BOOL result = BrotliEncoderCompressStream(state, encoding_op, &available_in,
		                                                 &next_in, &available_out, &next_out, NULL);

		if(!result) {
			LOG_MESSAGE_SIMPLE(LogLevelError,
			                   "An error in brotli compression processing occured\n");
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

		if(BrotliEncoderIsFinished(state)) {
			break;
		}

		//
	}

	BrotliEncoderDestroyInstance(state);
	return resultBuffer;
}
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD

#define ZSTD_COMPRESSION_LEVEL 10 // 1-22

#define ZSTD_CHUNK_SIZE 15 // not a windows size, just the chunk size

static SizedBuffer compress_buffer_with_zstd(SizedBuffer buffer) {

	ZSTD_CStream* stream = ZSTD_createCStream();

	if(!stream) {
		LOG_MESSAGE_SIMPLE(
		    LogLevelError,
		    "An error in zstd compression initiliaization occured: failed to initialize state\n");

		return SIZED_BUFFER_ERROR;
	}

	const size_t initResult = ZSTD_initCStream(stream, ZSTD_COMPRESSION_LEVEL);
	if(ZSTD_isError(initResult)) {
		LOG_MESSAGE(LogLevelError, "An error in zstd compression initiliaization occured: %s\n",
		            ZSTD_getErrorName(initResult));

		ZSTD_freeCStream(stream);
		return SIZED_BUFFER_ERROR;
	}

	ZSTD_inBuffer inputBuffer = { .src = buffer.data, .size = buffer.size, .pos = 0 };

	const size_t chunk_size = (1 << ZSTD_CHUNK_SIZE);

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		ZSTD_freeCStream(stream);
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer resultBuffer = { .data = start_chunk, .size = 0 };

	ZSTD_outBuffer outBuffer = { .dst = resultBuffer.data, .size = chunk_size, .pos = 0 };

	while(true) {
		ZSTD_EndDirective operation =
		    inputBuffer.pos != inputBuffer.size ? ZSTD_e_flush : ZSTD_e_end;

		const size_t ret = ZSTD_compressStream2(stream, &outBuffer, &inputBuffer, operation);

		if(ZSTD_isError(ret)) {
			LOG_MESSAGE(LogLevelError, "An error in zstd compression processing occured: %s\n",
			            ZSTD_getErrorName(initResult));

			ZSTD_freeCStream(stream);
			freeSizedBuffer(resultBuffer);
			return SIZED_BUFFER_ERROR;
		}

		resultBuffer.size = outBuffer.pos;

		if(outBuffer.size == outBuffer.pos) {
			void* new_chunk = realloc(resultBuffer.data, resultBuffer.size + chunk_size);
			resultBuffer.data = new_chunk;

			outBuffer.size += chunk_size;
		}

		if(operation != ZSTD_e_end) {
			continue;
		}

		if(inputBuffer.pos == inputBuffer.size) {
			break;
		}
	}

	ZSTD_freeCStream(stream);
	return resultBuffer;
}
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_COMPRESS

#ifdef NDEBUG
#define COMPRESS_QUIET true
#else
#define COMPRESS_QUIET false
#endif

#define COMPRESS_WINDOWS_SIZE 13

NODISCARD static const char* get_lzws_error(int error) {
	switch(error) {
		case 0: return "no error";
		case LZWS_COMPRESSOR_ALLOCATE_FAILED: return "allocate failed";
		case LZWS_COMPRESSOR_INVALID_MAX_CODE_BIT_LENGTH: return "invalid max code bit";
		case LZWS_COMPRESSOR_NEEDS_MORE_SOURCE: return "needs more source";
		case LZWS_COMPRESSOR_NEEDS_MORE_DESTINATION: return "needs more destination";
		case LZWS_COMPRESSOR_UNKNOWN_STATUS: return "unknown status";
		case LZWS_COMPRESSOR_UNKNOWN_ERROR:
		default: return "unknown error";
	}
}

static SizedBuffer compress_buffer_with_compress(SizedBuffer buffer) {
	lzws_compressor_state_t* compressor_state_ptr = NULL;

	// this needs to be like this for it to work!
	const lzws_compressor_options_t options = { .without_magic_header = false,
		                                        .max_code_bit_length =
		                                            LZWS_BIGGEST_MAX_CODE_BIT_LENGTH,
		                                        .block_mode = true,
		                                        .msb = false,
		                                        .unaligned_bit_groups = false,
		                                        .quiet = COMPRESS_QUIET };

	lzws_result_t result = lzws_compressor_get_initial_state(&compressor_state_ptr, &options);
	if(result != 0) {
		LOG_MESSAGE(LogLevelError,
		            "An error in compress compression initiliaization "
		            "occured: failed to initialize state: %s\n",
		            get_lzws_error(result));
		return SIZED_BUFFER_ERROR;
	}

	size_t chunk_size = 1 << COMPRESS_WINDOWS_SIZE;

	SizedBuffer compressor_buffer = { .data = NULL, .size = chunk_size };

	result = lzws_create_destination_buffer_for_compressor((lzws_byte_t**)&compressor_buffer.data,
	                                                       &compressor_buffer.size, COMPRESS_QUIET);
	if(result != 0) {
		LOG_MESSAGE(LogLevelError,
		            "An error in compress compression initiliaization "
		            "occured: create destination buffer for compressor failed: %s\n",
		            get_lzws_error(result));
		lzws_compressor_free_state(compressor_state_ptr);
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer inputBuffer = sized_buffer_get_exact_clone(buffer);
	SizedBuffer remaining_compressor_buffer = sized_buffer_get_exact_clone(compressor_buffer);

	while(true) {

		result = lzws_compress(compressor_state_ptr, (lzws_byte_t**)&inputBuffer.data,
		                       &inputBuffer.size, (lzws_byte_t**)&remaining_compressor_buffer.data,
		                       &remaining_compressor_buffer.size);

		if(result == LZWS_COMPRESSOR_NEEDS_MORE_DESTINATION) {
			size_t current_size = compressor_buffer.size - remaining_compressor_buffer.size;

			void* new_chunk = realloc(compressor_buffer.data, current_size + chunk_size);
			compressor_buffer.data = new_chunk;
			compressor_buffer.size = current_size + chunk_size;

			remaining_compressor_buffer.data = (lzws_byte_t*)compressor_buffer.data + current_size;
			remaining_compressor_buffer.size = chunk_size;

			continue;
		}

		if(result != 0) {
			LOG_MESSAGE(LogLevelError,
			            "An error in compress compression processing occured: compress state: %s\n",
			            get_lzws_error(result));
			freeSizedBuffer(compressor_buffer);
			lzws_compressor_free_state(compressor_state_ptr);
			return SIZED_BUFFER_ERROR;
		}

		if(inputBuffer.size == 0) {
			break;
		}
	}

	result = lzws_compressor_finish(compressor_state_ptr,
	                                (lzws_byte_t**)&remaining_compressor_buffer.data,
	                                &remaining_compressor_buffer.size);

	if(result != 0) {
		LOG_MESSAGE(LogLevelError,
		            "An error in compress compression processing occured: finish state: %s\n",
		            get_lzws_error(result));

		freeSizedBuffer(compressor_buffer);
		lzws_compressor_free_state(compressor_state_ptr);
		return SIZED_BUFFER_ERROR;
	}

	lzws_compressor_free_state(compressor_state_ptr);

	SizedBuffer resultBuffer = { .data = compressor_buffer.data,
		                         .size =
		                             compressor_buffer.size - remaining_compressor_buffer.size };

	return resultBuffer;

	//
}
#endif

NODISCARD const char* get_string_for_compress_format(COMPRESSION_TYPE format) {

	switch(format) {
		case COMPRESSION_TYPE_NONE: return "none";
		case COMPRESSION_TYPE_GZIP: return "gzip";
		case COMPRESSION_TYPE_DEFLATE: return "deflate";
		case COMPRESSION_TYPE_BR: return "br";
		case COMPRESSION_TYPE_ZSTD: return "zstd";
		case COMPRESSION_TYPE_COMPRESS: return "compress";
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
		case COMPRESSION_TYPE_COMPRESS: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_COMPRESS
			return compress_buffer_with_compress(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		default: {
			UNUSED(buffer);
			return SIZED_BUFFER_ERROR;
		}
	}
}
