

#pragma once

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

void set_hpack_state_setting(HpackState* state, size_t max_dynamic_table_byte_size);

void global_initialize_http2_hpack_data(void);

void global_free_http2_hpack_data(void);
