
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/sized_buffer.h"
#include "utils/utils.h"
#include "generated_hpack_huffman.h"

typedef struct {
	bool is_error;
	union {
		SizedBuffer result;
		const char* error;
	} data;
} HuffmanResult;

NODISCARD HuffmanResult apply_huffman_code(const HuffManTree* tree, SizedBuffer input);

void global_initialize_http2_hpack_huffman_data(void);

void global_free_http2_hpack_huffman_data(void);

#ifdef __cplusplus
}
#endif
