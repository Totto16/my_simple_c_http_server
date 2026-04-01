

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "http/protocol.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct HpackDecompressStateImpl HpackDecompressState;

NODISCARD HpackDecompressState*
get_default_hpack_decompress_state(size_t max_dynamic_table_byte_size);

void free_hpack_decompress_state(HpackDecompressState* decompress_state);

GENERATE_VARIANT_ALL_HTTP2_HPACK_DECOMPRESS_RESULT()

NODISCARD Http2HpackDecompressResult
http2_hpack_decompress_data(HpackDecompressState* decompress_state, ReadonlyBuffer input);

typedef struct HpackCompressStateImpl HpackCompressState;

NODISCARD HpackCompressState* get_default_hpack_compress_state(size_t max_dynamic_table_byte_size);

void free_hpack_compress_state(HpackCompressState* compress_state);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2HpackHuffmanUsageAuto = 0,
	Http2HpackHuffmanUsageAlways,
	Http2HpackHuffmanUsageNever,
} Http2HpackHuffmanUsage;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2HpackCompressTypeNoTableUsage = 0,
	Http2HpackCompressTypeStaticTableUsage,
	Http2HpackCompressTypeAllTablesUsage,
} Http2HpackCompressType;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2HpackTableAddTypeNone = 0,
	Http2HpackTableAddTypeCommon,
	Http2HpackTableAddTypeAll,
} Http2HpackTableAddType;

typedef struct {
	Http2HpackHuffmanUsage huffman_usage;
	Http2HpackCompressType type;
	Http2HpackTableAddType table_add_type;
} Http2HpackCompressOptions;

NODISCARD SizedBuffer http2_hpack_compress_data(HpackCompressState* compress_state,
                                                HttpHeaderFields header_fields,
                                                Http2HpackCompressOptions options);

void set_hpack_decompress_state_setting(HpackDecompressState* decompress_state,
                                        size_t max_dynamic_table_byte_size);

void set_hpack_compress_state_setting(HpackCompressState* compress_state,
                                      size_t max_dynamic_table_byte_size);

typedef uint64_t HpackVariableInteger;

typedef struct {
	bool is_error;
	union {
		HpackVariableInteger value;
		tstr_static error;
	} data;
} HpackVariableIntegerResult;

// this is only public for tests, are there better ways to only expose it for tests?
NODISCARD HpackVariableIntegerResult decode_hpack_variable_integer(size_t* pos, size_t size,
                                                                   const uint8_t* data,
                                                                   uint8_t prefix_bits);

void global_initialize_http2_hpack_data(void);

void global_free_http2_hpack_data(void);

#ifdef __cplusplus
}
#endif
