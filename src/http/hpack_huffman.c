
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

NODISCARD HuffmanResult decode_bytes_huffman(const HuffManTree* const tree,
                                             const SizedBuffer input) {

	if(tree == NULL) {
		return (HuffmanResult){ .is_error = true, .data = { .error = "tree is NULL" } };
	}

	if(input.size == 0 || input.data == NULL) {
		return (HuffmanResult){ .is_error = true, .data = { .error = "input is NULL or empty" } };
	}

	size_t memory_size =
	    (((input.size * 8) + (MIN_HPACK_BITS_PER_CHAR - 1)) / MIN_HPACK_BITS_PER_CHAR);

	uint8_t* const values = malloc(memory_size + 1);

	if(values == NULL) {
		return (HuffmanResult){ .is_error = true, .data = { .error = "failed malloc" } };
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
			return (HuffmanResult){ .is_error = true, .data = { .error = next_node->data.error } };
		}

		if(next_node->type == HuffManNodeTypeEnd) {
			values[values_idx] = next_node->data.end;
			++values_idx;

			if(values_idx >= memory_size) {
				// out of bounds on output
				free(values);
				return (HuffmanResult){ .is_error = true,
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
		return (HuffmanResult){ .is_error = true,
			                    .data = {
			                        .error = "more than one byte not decoded, invalid decoding" } };
	}

	size_t bits_not_decoded = 8 - last_pos.bits_pos;

	if(bits_not_decoded >= 8) {
		// 8 bits not decoded, is also invalid
		return (HuffmanResult){
			.is_error = true, .data = { .error = "8 or more bits not decoded, is also invalid" }
		};
	}

	uint8_t bits_mask = ((1 << bits_not_decoded) - 1);

	if((data[last_pos.pos] & bits_mask) != (EOS_BYTE & bits_mask)) {
		// last not decoded bits are not the eos bytes
		return (HuffmanResult){
			.is_error = true, .data = { .error = "last not decoded bits are not the EOS bytes" }
		};
	}

huffman_return_ok:

	// resize the actual buffer
	void* new_values = realloc(values, values_idx);

	if(new_values == NULL) {
		return (HuffmanResult){
			.is_error = true,
			.data = { .error = "realloc to smaller size failed, LOL, you really got unlucky xD" }
		};
	}

	return (HuffmanResult){ .is_error = false,
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

NODISCARD HuffmanResult decode_bytes_huffman_with_global_data_setup(const SizedBuffer input) {
	if(g_huffman_tree_data.tree == NULL) {
		return (HuffmanResult){ .is_error = true,
			                    .data = { .error = "global tree is not initialized" } };
	}

	return decode_bytes_huffman(g_huffman_tree_data.tree, input);
}
