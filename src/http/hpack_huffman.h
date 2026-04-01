
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "variants.h"

#include "generated_hpack_huffman.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"
#include <tstr.h>

GENERATE_VARIANT_ALL_HUFFMAN_DECODE_RESULT()

NODISCARD HuffmanDecodeResult hpack_huffman_decode_bytes(ReadonlyBuffer buffer);

NODISCARD size_t hpack_huffman_get_encoded_size(const tstr* str);

GENERATE_VARIANT_ALL_HUFFMAN_ENCODE_FIXED_RESULT()

NODISCARD HuffmanEncodeFixedResult hpack_huffman_encode_value_fixed_size(void* data,
                                                                         size_t max_size,
                                                                         const tstr* str);

GENERATE_VARIANT_ALL_HUFFMAN_ENCODE_RESULT()

NODISCARD HuffmanEncodeResult hpack_huffman_encode_value(const tstr* str);

void global_initialize_http2_hpack_huffman_data(void);

void global_free_http2_hpack_huffman_data(void);

#ifdef __cplusplus
}
#endif
