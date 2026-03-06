
#include "./hpack_huffman.h"

// the EOS byte is all ones, as it can be at the end of an octet, it can only be 7 bits long at the
// most, 8 bits or more i an decoding error, if it is decoeded by the tree, it is an error too!
#define EOS_BYTE 0xFF

// the static table has a few 5 bit encodings, that is the minimum, this is used for the memory
// allocation
#define MIN_HPACK_BITS_PER_CHAR 5

typedef struct {
	size_t pos;
	uint8_t bits_pos; // 0-7
} BitPos;

static void bit_pos_inc(BitPos* const bit_pos) {

	bit_pos->bits_pos++;

	if(bit_pos->bits_pos >= 8) {
		bit_pos->bits_pos = 0;
		bit_pos->pos++;
	}
}

NODISCARD static HuffmanDecodeResult decode_bytes_huffman_impl(const HuffManTree* const tree,
                                                               const SizedBuffer input) {

	if(tree == NULL) {
		return (HuffmanDecodeResult){ .is_error = true, .data = { .error = "tree is NULL" } };
	}

	if(input.size == 0 || input.data == NULL) {
		return (HuffmanDecodeResult){ .is_error = true,
			                          .data = { .error = "input is NULL or empty" } };
	}

	size_t memory_size =
	    (((input.size * 8) + (MIN_HPACK_BITS_PER_CHAR - 1)) / MIN_HPACK_BITS_PER_CHAR);

	uint8_t* const values = malloc(memory_size + 1);

	if(values == NULL) {
		return (HuffmanDecodeResult){ .is_error = true, .data = { .error = "failed malloc" } };
	}

	memset(values, '\0', memory_size);

	// the index gets the bit at position of that index
	const uint8_t bit_pos_mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	const size_t size = input.size;
	const uint8_t* const data = (uint8_t*)input.data;

	size_t values_idx = 0;

	BitPos current_pos = { .pos = 0, .bits_pos = 0 };

	BitPos last_pos = current_pos;

	const HuffManNode* current_node = tree->root;

	while(current_pos.pos < size) {
		assert(current_node->type == HuffManNodeTypeNode);

		const bool bit = ((data[current_pos.pos]) & (bit_pos_mask[current_pos.bits_pos])) != 0;

		const HuffManNode* const next_node =
		    bit ? current_node->data.node.bit_1 : current_node->data.node.bit_0;

		if(next_node->type == HuffManNodeTypeError) {
			free(values);
			return (HuffmanDecodeResult){ .is_error = true,
				                          .data = { .error = next_node->data.error } };
		}

		if(next_node->type == HuffManNodeTypeEnd) {
			values[values_idx] = next_node->data.end;
			++values_idx;

			if(values_idx >= memory_size) {
				// out of bounds on output
				free(values);
				return (HuffmanDecodeResult){ .is_error = true,
					                          .data = { .error = "result memory overflow" } };
			}

			current_node = tree->root;

			bit_pos_inc(&current_pos);

			last_pos = current_pos;
		} else {
			current_node = next_node;

			bit_pos_inc(&current_pos);
		}
	}

	if(current_node == tree->root) {
		// we have encoded it until the last bit, it is valid

		goto huffman_return_ok;
	}

	// we have some bytes / bits left

	if(size >= last_pos.pos) {
		// no bits left, but should be caught by the previous if

		goto huffman_return_ok;
	}

	size_t bytes_not_decoded = size - last_pos.pos;

	if(bytes_not_decoded > 1) {
		// more than one byte not decoded, invalid decoding
		return (HuffmanDecodeResult){
			.is_error = true,
			.data = { .error = "more than one byte not decoded, invalid decoding" }
		};
	}

	size_t bits_not_decoded = 8 - last_pos.bits_pos;

	if(bits_not_decoded >= 8) {
		// 8 bits not decoded, is also invalid
		return (HuffmanDecodeResult){
			.is_error = true, .data = { .error = "8 or more bits not decoded, is also invalid" }
		};
	}

	uint8_t bits_mask = ((1 << bits_not_decoded) - 1);

	if((data[last_pos.pos] & bits_mask) != (EOS_BYTE & bits_mask)) {
		// last not decoded bits are not the eos bytes
		return (HuffmanDecodeResult){
			.is_error = true, .data = { .error = "last not decoded bits are not the EOS bytes" }
		};
	}

huffman_return_ok:

	// resize the actual buffer
	void* new_values = realloc(values, values_idx + 1);

	if(new_values == NULL) {
		return (HuffmanDecodeResult){
			.is_error = true,
			.data = { .error = "realloc to smaller size failed, LOL, you really got unlucky xD" }
		};
	}

	return (HuffmanDecodeResult){ .is_error = false,
		                          .data = { .result = (SizedBuffer){ .data = new_values,
		                                                             .size = values_idx } } };
}

typedef struct {
	HuffManTree* tree;
} GlobalHpackHuffmanData;

GlobalHpackHuffmanData
    g_huffman_tree_data = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    { .tree = NULL };

void global_initialize_http2_hpack_huffman_data(void) {
	g_huffman_tree_data.tree = get_hpack_huffman_tree();
}

void global_free_http2_hpack_huffman_data(void) {
	free_hpack_huffman_tree(g_huffman_tree_data.tree);
}

NODISCARD HuffmanDecodeResult decode_bytes_huffman(const SizedBuffer input) {
	if(g_huffman_tree_data.tree == NULL) {
		return (HuffmanDecodeResult){ .is_error = true,
			                          .data = { .error = "global tree is not initialized" } };
	}

	return decode_bytes_huffman_impl(g_huffman_tree_data.tree, input);
}

NODISCARD size_t http_hpack_get_huffman_encoded_size(const tstr* const str) {

	size_t result_bits = 0;

	uint8_t* str_ptr = (uint8_t*)tstr_cstr(str);

	for(size_t i = 0; i < tstr_len(str); ++i) {
		const uint8_t value = str_ptr[i];
		result_bits += g_huffman_encode_map[value].bit_size;
	}

	// do + 7 to ceil it in a simple fashion
	return (result_bits + 7) / 8;
}

NODISCARD HuffmanEncodeResult http_hpack_encode_value_fixed_size(void* const data,
                                                                 const size_t max_size,
                                                                 const tstr* const str) {

	const size_t str_len = tstr_len(str);

	if(str_len == 0) {
		return (HuffmanEncodeResult){ .is_error = false, .data = { .result_size = 0 } };
	}

	if(max_size == 0) {
		return (HuffmanEncodeResult){ .is_error = true, .data = { .error = "max size is 0" } };
	}

	uint8_t* data_ptr = (uint8_t*)data;

	uint8_t* str_ptr = (uint8_t*)tstr_cstr(str);

	BitPos current_pos = { .pos = 0, .bits_pos = 0 };

	for(size_t i = 0; i < str_len; ++i) {
		const uint8_t char_val = str_ptr[i];
		const HuffmanEncodeEntry encode_entry = g_huffman_encode_map[char_val];

		uint32_t value = encode_entry.value;

		for(size_t j = 0; j < encode_entry.bit_size; ++j) {
			if(current_pos.bits_pos == 0) {
				// initialize to 0
				data_ptr[current_pos.pos] = 0;
			}

			const uint8_t bit = value & 0x01;
			value = value >> 1;

			data_ptr[current_pos.pos] =
			    data_ptr[current_pos.pos] | (bit << (7 - current_pos.bits_pos));

			bit_pos_inc(&current_pos);
		}

		// encode if possible, advance position
	}

	if(current_pos.bits_pos > 0) {
		const size_t missing_bits = 8 - current_pos.bits_pos;
		// set the last bits to EOS bits (all 1s)
		data_ptr[current_pos.pos] = data_ptr[current_pos.pos] | ((1 << missing_bits) - 1);

		current_pos.bits_pos = 0;
		current_pos.pos += 1;
	}

	return (HuffmanEncodeResult){ .is_error = false, .data = { .result_size = current_pos.pos } };
}
