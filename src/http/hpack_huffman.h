
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "generated_hpack_huffman.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"
#include <tstr.h>

typedef struct {
	bool is_error;
	union {
		SizedBuffer result;
		const char* error;
	} data;
} HuffmanDecodeResult;

NODISCARD HuffmanDecodeResult hpack_huffman_decode_bytes(const void* data, size_t size);

NODISCARD size_t hpack_huffman_get_encoded_size(const tstr* str);

typedef struct {
	bool is_error;
	union {
		size_t result_size;
		const char* error;
	} data;
} HuffmanEncodeFixedResult;

NODISCARD HuffmanEncodeFixedResult hpack_huffman_encode_value_fixed_size(void* data,
                                                                         size_t max_size,
                                                                         const tstr* str);

typedef struct {
	bool is_error;
	union {
		SizedBuffer result;
		const char* error;
	} data;
} HuffmanEncodeResult;

NODISCARD HuffmanEncodeResult hpack_huffman_encode_value(const tstr* str);

void global_initialize_http2_hpack_huffman_data(void);

void global_free_http2_hpack_huffman_data(void);

#ifdef __cplusplus
}
#endif
