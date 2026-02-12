
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

NODISCARD SizedBuffer apply_huffman_code(const HuffManTree* const tree, SizedBuffer input) {

	if(tree == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	if(input.size == 0 || input.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	size_t memory_size =
	    (((input.size * 8) + (MIN_HPACK_BITS_PER_CHAR - 1)) / MIN_HPACK_BITS_PER_CHAR);

	uint8_t* const values = malloc(memory_size + 1);

	if(values == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
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
			// TODO: log error
			free(values);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		if(next_node->type == HuffManNodeTypeEnd) {
			values[values_idx] = next_node->data.end;
			++values_idx;

			if(values_idx >= memory_size) {
				// out of bounds on output
				free(values);
				return (SizedBuffer){ .data = NULL, .size = 0 };
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

		return (SizedBuffer){ .data = values, .size = values_idx };
	}

	// we have some bytes / bits left

	if(size >= last_pos.pos) {
		// no bits left, but should be caught by the previous if

		return (SizedBuffer){ .data = values, .size = values_idx };
	}

	size_t bytes_not_decoded = size - last_pos.pos;

	if(bytes_not_decoded > 1) {
		// more than one byte not decoded, invalid decoding
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	size_t bits_not_decoded = 8 - last_pos.bits_pos;

	if(bits_not_decoded >= 8) {
		// 8 bits not decoded, is also invalid
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t bits_mask = ((1 << bits_not_decoded) - 1);

	if((data[last_pos.pos] & bits_mask) != (EOS_BYTE & bits_mask)) {
		// last not decoded bits are not the eos bytes
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	return (SizedBuffer){ .data = values, .size = values_idx };
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
