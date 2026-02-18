

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "http/protocol.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct HpackStateImpl HpackState;

NODISCARD HpackState* get_default_hpack_state(size_t max_dynamic_table_byte_size);

void free_hpack_state(HpackState* state);

typedef struct {
	bool is_error;
	union {
		HttpHeaderFields result;
		const char* error;
	} data;
} Http2HpackDecompressResult;

NODISCARD Http2HpackDecompressResult http2_hpack_decompress_data(HpackState* state,
                                                                 SizedBuffer input);

NODISCARD SizedBuffer http2_hpack_compress_data(HpackState* state, HttpHeaderFields header_fields);

void set_hpack_state_setting(HpackState* state, size_t max_dynamic_table_byte_size);

typedef uint64_t HpackVariableInteger;

typedef struct {
	bool is_error;
	union {
		HpackVariableInteger value;
		const char* error;
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
