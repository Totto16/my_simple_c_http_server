
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

bool is_compressions_supported(CompressionType format) {

	switch(format) {
		case CompressionTypeNone: { // NOLINT(bugprone-branch-clone)
			return true;
		}
		case CompressionTypeGzip: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
			return true;
#else
			return false;
#endif
		}
		case CompressionTypeDeflate: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
			return true;
#else
			return false;
#endif
		}
		case CompressionTypeBr: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
			return true;
#else
			return false;
#endif
		}
		case CompressionTypeZstd: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD
			return true;
#else
			return false;
#endif
		}
		case CompressionTypeCompress: {
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

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ZlibModeGzip = 0,
	ZlibModeDeflate,
	ZlibModeDeflateRaw,
} ZlibMode;

#define Z_MEMORY_USAGE_LEVEL 8 // 1-9

#define Z_MIN_WINDOW_BITS 8
#define Z_MAX_WINDOW_BITS 15

#define Z_GZIP_ENCODING 16 // not inside the zlib header, unfortunately

// see https://zlib.net/manual.html
NODISCARD static SizedBuffer
compress_buffer_with_zlib_impl(SizedBuffer buffer,
                               ZlibMode mode, // NOLINT(bugprone-easily-swappable-parameters)
                               size_t max_window_bits, int flush_mode) {

	if(max_window_bits < Z_MIN_WINDOW_BITS || max_window_bits > Z_MAX_WINDOW_BITS) {
		return get_empty_sized_buffer();
	}

	int window_bits = (int)max_window_bits;

	switch(mode) {
		case ZlibModeGzip: {
			window_bits = window_bits | Z_GZIP_ENCODING;
			break;
		}
		case ZlibModeDeflate: {
			break;
		}
		case ZlibModeDeflateRaw: {
			window_bits = -window_bits;
			break;
		}
		default: {
			return get_empty_sized_buffer();
		}
	}

	// the maximum window bits are used, even if we could use loweer widnow bits

	const size_t chunk_size = 1UL << max_window_bits;

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer result_buffer = { .data = start_chunk, .size = 0 };

	z_stream zstream = {};
	zstream.zalloc = Z_NULL;
	zstream.zfree = Z_NULL;
	zstream.opaque = Z_NULL;

	zstream.avail_in = buffer.size;
	zstream.next_in = (Bytef*)buffer.data;
	zstream.avail_out = chunk_size;
	zstream.next_out = (Bytef*)result_buffer.data;

	int result = deflateInit2(&zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window_bits,
	                          Z_MEMORY_USAGE_LEVEL, Z_DEFAULT_STRATEGY);

	if(result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in zlib compression initialization occurred: %s\n",
		            zError(result));
		free_sized_buffer(result_buffer);

		return SIZED_BUFFER_ERROR;
	}

	bool final_pass = false;

	while(true) {
		int deflate_result = deflate(
		    &zstream,
		    final_pass ? Z_FINISH : flush_mode); // NOLINT(readability-implicit-bool-conversion)

		if(!final_pass) {
			result_buffer.size += (chunk_size - zstream.avail_out);
		} else {
			result_buffer.size = zstream.total_out;
		}

		if(deflate_result == Z_STREAM_END) {
			break;
		}

		if((deflate_result == Z_BUF_ERROR || deflate_result == Z_OK) && zstream.avail_out == 0) {

			void* new_chunk = realloc(result_buffer.data, result_buffer.size + chunk_size);
			result_buffer.data = new_chunk;

			zstream.avail_out = chunk_size;
			zstream.next_out = (Bytef*)new_chunk + result_buffer.size;
			continue;
		}

		if(deflate_result == Z_OK) {
			final_pass = true;
			continue;
		}

		LOG_MESSAGE(LogLevelError, "An error in zlib compression processing occurred: %s\n",
		            zError(deflate_result));
		free_sized_buffer(result_buffer);

		return SIZED_BUFFER_ERROR;
	}

	assert(result_buffer.size == zstream.total_out);

	int deflate_end_result = deflateEnd(&zstream);

	if(deflate_end_result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in zlib compression stream end occurred: %s\n",
		            zError(deflate_end_result));
		free_sized_buffer(result_buffer);

		return SIZED_BUFFER_ERROR;
	}

	return result_buffer;
}

#define Z_DEFAULT_WINDOW_SIZE 15 // 8-15

NODISCARD static SizedBuffer compress_buffer_with_zlib_compat(SizedBuffer buffer, bool gzip) {

	return compress_buffer_with_zlib_impl(
	    buffer,
	    gzip ? ZlibModeGzip : ZlibModeDeflate, // NOLINT(readability-implicit-bool-conversion)
	    Z_DEFAULT_WINDOW_SIZE, Z_FINISH);
}

#define WS_FLUSH_MODE Z_SYNC_FLUSH

NODISCARD SizedBuffer compress_buffer_with_zlib_for_ws(SizedBuffer buffer, size_t max_window_bits) {
	return compress_buffer_with_zlib_impl(buffer, ZlibModeDeflateRaw, max_window_bits,
	                                      WS_FLUSH_MODE);
}

// see https://zlib.net/manual.html
NODISCARD static SizedBuffer
decompress_buffer_with_zlib_impl(SizedBuffer buffer,
                                 ZlibMode mode, // NOLINT(bugprone-easily-swappable-parameters)
                                 size_t max_window_bits, int flush_mode) {

	if(max_window_bits < Z_MIN_WINDOW_BITS || max_window_bits > Z_MAX_WINDOW_BITS) {
		return SIZED_BUFFER_ERROR;
	}

	int window_bits = (int)max_window_bits;

	switch(mode) {
		case ZlibModeGzip: {
			window_bits = window_bits | Z_GZIP_ENCODING;
			break;
		}
		case ZlibModeDeflate: {
			break;
		}
		case ZlibModeDeflateRaw: {
			window_bits = -window_bits;
			break;
		}
		default: {
			return get_empty_sized_buffer();
		}
	}

	// the maximum window bits are used, even if we could use lower widnow bits

	const size_t chunk_size = 1UL << max_window_bits;

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer result_buffer = { .data = start_chunk, .size = 0 };

	z_stream zstream = {};
	zstream.zalloc = Z_NULL;
	zstream.zfree = Z_NULL;
	zstream.opaque = Z_NULL;

	zstream.avail_in = buffer.size;
	zstream.next_in = (Bytef*)buffer.data;
	zstream.avail_out = chunk_size;
	zstream.next_out = (Bytef*)result_buffer.data;

	int result = inflateInit2(&zstream, window_bits);

	if(result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in zlib decompression initialization occurred: %s\n",
		            zError(result));
		free_sized_buffer(result_buffer);

		return SIZED_BUFFER_ERROR;
	}

	while(true) {
		int inflate_result = inflate(&zstream, flush_mode);

		result_buffer.size += (chunk_size - zstream.avail_out);

		if(inflate_result == Z_STREAM_END) {
			break;
		}

		if((inflate_result == Z_BUF_ERROR || inflate_result == Z_OK) && zstream.avail_out == 0) {
			if(zstream.avail_in == 0 && inflate_result == Z_OK) {
				// as we don't need more output, this buffer is enough, we are done
				break;
			}

			void* new_chunk = realloc(result_buffer.data, result_buffer.size + chunk_size);
			if(!new_chunk) {
				free_sized_buffer(result_buffer);
				return SIZED_BUFFER_ERROR;
			}
			result_buffer.data = new_chunk;

			zstream.avail_out = chunk_size;
			zstream.next_out = (Bytef*)new_chunk + result_buffer.size;
			continue;
		}

		if(inflate_result == Z_OK) {
			// don't need more buffer, so its ended
			break;
		}

		LOG_MESSAGE(LogLevelError, "An error in zlib decompression processing occurred: %s\n",
		            zError(inflate_result));
		free_sized_buffer(result_buffer);

		return SIZED_BUFFER_ERROR;
	}

	assert(result_buffer.size == zstream.total_out);

	int inflate_end_result = inflateEnd(&zstream);

	if(inflate_end_result != Z_OK) {
		LOG_MESSAGE(LogLevelError, "An error in zlib decompression stream end occurred: %s\n",
		            zError(inflate_end_result));
		free_sized_buffer(result_buffer);

		return SIZED_BUFFER_ERROR;
	}

	return result_buffer;
}
NODISCARD SizedBuffer decompress_buffer_with_zlib_for_ws(SizedBuffer buffer,
                                                         size_t max_window_bits) {
	return decompress_buffer_with_zlib_impl(buffer, ZlibModeDeflateRaw, max_window_bits,
	                                        WS_FLUSH_MODE);
}

#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
static SizedBuffer compress_buffer_with_gzip(SizedBuffer buffer) {
	return compress_buffer_with_zlib_compat(buffer, true);
}
#endif

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
static SizedBuffer compress_buffer_with_deflate(SizedBuffer buffer) {
	return compress_buffer_with_zlib_compat(buffer, false);
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
		    "An error in brotli compression initialization occurred: failed to initialize state\n");

		return SIZED_BUFFER_ERROR;
	}

	if(!BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, BROTLI_QUALITY)) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "An error in brotli compression initialization occurred: "
		                                  "failed to set parameter quality\n");

		return SIZED_BUFFER_ERROR;
	};

	if(!BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, BROTLI_WINDOW_SIZE)) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "An error in brotli compression initialization occurred: "
		                                  "failed to set parameter sliding window size\n");

		return SIZED_BUFFER_ERROR;
	}; // 0-11

	const size_t chunk_size = (1 << BROTLI_WINDOW_SIZE) - 16;

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		BrotliEncoderDestroyInstance(state);
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer result_buffer = { .data = start_chunk, .size = 0 };

	size_t available_in = buffer.size;

	const uint8_t* next_in = buffer.data;

	size_t available_out = chunk_size;

	uint8_t* next_out = result_buffer.data;

	while(true) {

		BrotliEncoderOperation encoding_op =
		    available_in == 0 ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;

		const size_t available_out_before = available_out;

		BROTLI_BOOL result = BrotliEncoderCompressStream(state, encoding_op, &available_in,
		                                                 &next_in, &available_out, &next_out, NULL);

		if(!result) {
			LOG_MESSAGE_SIMPLE(LogLevelError,
			                   "An error in brotli compression processing occurred\n");
			free_sized_buffer(result_buffer);
			BrotliEncoderDestroyInstance(state);

			return SIZED_BUFFER_ERROR;
		}

		result_buffer.size += (available_out_before - available_out);

		if(available_out == 0) {
			void* new_chunk = realloc(result_buffer.data, result_buffer.size + chunk_size);
			result_buffer.data = new_chunk;

			available_out += chunk_size;
			next_out = (uint8_t*)new_chunk + result_buffer.size;
		}

		if(BrotliEncoderIsFinished(state)) {
			break;
		}

		//
	}

	BrotliEncoderDestroyInstance(state);
	return result_buffer;
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
		    "An error in zstd compression initialization occurred: failed to initialize state\n");

		return SIZED_BUFFER_ERROR;
	}

	const size_t init_result = ZSTD_initCStream(stream, ZSTD_COMPRESSION_LEVEL);
	if(ZSTD_isError(init_result)) {
		LOG_MESSAGE(LogLevelError, "An error in zstd compression initialization occurred: %s\n",
		            ZSTD_getErrorName(init_result));

		ZSTD_freeCStream(stream);
		return SIZED_BUFFER_ERROR;
	}

	ZSTD_inBuffer input_buffer = { .src = buffer.data, .size = buffer.size, .pos = 0 };

	const size_t chunk_size = (1 << ZSTD_CHUNK_SIZE);

	void* start_chunk = malloc(chunk_size);
	if(!start_chunk) {
		ZSTD_freeCStream(stream);
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer result_buffer = { .data = start_chunk, .size = 0 };

	ZSTD_outBuffer out_buffer = { .dst = result_buffer.data, .size = chunk_size, .pos = 0 };

	while(true) {
		ZSTD_EndDirective operation =
		    input_buffer.pos != input_buffer.size ? ZSTD_e_flush : ZSTD_e_end;

		const size_t ret = ZSTD_compressStream2(stream, &out_buffer, &input_buffer, operation);

		if(ZSTD_isError(ret)) {
			LOG_MESSAGE(LogLevelError, "An error in zstd compression processing occurred: %s\n",
			            ZSTD_getErrorName(init_result));

			ZSTD_freeCStream(stream);
			free_sized_buffer(result_buffer);
			return SIZED_BUFFER_ERROR;
		}

		result_buffer.size = out_buffer.pos;

		if(out_buffer.size == out_buffer.pos) {
			void* new_chunk = realloc(result_buffer.data, result_buffer.size + chunk_size);
			result_buffer.data = new_chunk;

			out_buffer.size += chunk_size;
		}

		if(operation != ZSTD_e_end) {
			continue;
		}

		if(input_buffer.pos == input_buffer.size) {
			break;
		}
	}

	ZSTD_freeCStream(stream);
	return result_buffer;
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
		            "An error in compress compression initialization "
		            "occurred: failed to initialize state: %s\n",
		            get_lzws_error(result));
		return SIZED_BUFFER_ERROR;
	}

	size_t chunk_size = 1 << COMPRESS_WINDOWS_SIZE;

	SizedBuffer compressor_buffer = { .data = NULL, .size = chunk_size };

	result = lzws_create_destination_buffer_for_compressor((lzws_byte_t**)&compressor_buffer.data,
	                                                       &compressor_buffer.size, COMPRESS_QUIET);
	if(result != 0) {
		LOG_MESSAGE(LogLevelError,
		            "An error in compress compression initialization "
		            "occurred: create destination buffer for compressor failed: %s\n",
		            get_lzws_error(result));
		lzws_compressor_free_state(compressor_state_ptr);
		return SIZED_BUFFER_ERROR;
	}

	SizedBuffer input_buffer = sized_buffer_get_exact_clone(buffer);
	SizedBuffer remaining_compressor_buffer = sized_buffer_get_exact_clone(compressor_buffer);

	while(true) {

		result = lzws_compress(compressor_state_ptr, (lzws_byte_t**)&input_buffer.data,
		                       &input_buffer.size, (lzws_byte_t**)&remaining_compressor_buffer.data,
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
			LOG_MESSAGE(
			    LogLevelError,
			    "An error in compress compression processing occurred: compress state: %s\n",
			    get_lzws_error(result));
			free_sized_buffer(compressor_buffer);
			lzws_compressor_free_state(compressor_state_ptr);
			return SIZED_BUFFER_ERROR;
		}

		if(input_buffer.size == 0) {
			break;
		}
	}

	result = lzws_compressor_finish(compressor_state_ptr,
	                                (lzws_byte_t**)&remaining_compressor_buffer.data,
	                                &remaining_compressor_buffer.size);

	if(result != 0) {
		LOG_MESSAGE(LogLevelError,
		            "An error in compress compression processing occurred: finish state: %s\n",
		            get_lzws_error(result));

		free_sized_buffer(compressor_buffer);
		lzws_compressor_free_state(compressor_state_ptr);
		return SIZED_BUFFER_ERROR;
	}

	lzws_compressor_free_state(compressor_state_ptr);

	SizedBuffer result_buffer = { .data = compressor_buffer.data,
		                          .size =
		                              compressor_buffer.size - remaining_compressor_buffer.size };

	return result_buffer;

	//
}
#endif

NODISCARD const char* get_string_for_compress_format(CompressionType format) {

	switch(format) {
		case CompressionTypeNone: return "none";
		case CompressionTypeGzip: return "gzip";
		case CompressionTypeDeflate: return "deflate";
		case CompressionTypeBr: return "br";
		case CompressionTypeZstd: return "zstd";
		case CompressionTypeCompress: return "compress";
		default: return "<unknown>";
	}
}

NODISCARD SizedBuffer compress_buffer_with(SizedBuffer buffer, CompressionType format) {

	switch(format) {
		case CompressionTypeNone: return SIZED_BUFFER_ERROR; ;
		case CompressionTypeGzip: {

#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_GZIP
			return compress_buffer_with_gzip(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case CompressionTypeDeflate: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_DEFLATE
			return compress_buffer_with_deflate(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case CompressionTypeBr: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_BR
			return compress_buffer_with_br(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case CompressionTypeZstd: {
#ifdef _SIMPLE_SERVER_COMPRESSION_SUPPORT_ZSTD
			return compress_buffer_with_zstd(buffer);
#else
			return SIZED_BUFFER_ERROR;
#endif
		};
		case CompressionTypeCompress: {
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
