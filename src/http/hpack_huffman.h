
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "generated_hpack_huffman.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct {
	bool is_error;
	union {
		SizedBuffer result;
		const char* error;
	} data;
} HuffmanResult;

NODISCARD HuffmanResult decode_bytes_huffman(const HuffManTree* tree, SizedBuffer input);

NODISCARD HuffmanResult decode_bytes_huffman_with_global_data_setup(SizedBuffer input);

void global_initialize_http2_hpack_huffman_data(void);

void global_free_http2_hpack_huffman_data(void);

#ifdef __cplusplus
}
#endif
