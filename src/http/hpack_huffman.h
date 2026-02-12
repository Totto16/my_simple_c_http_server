
#pragma once

#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct HuffManTreeImpl HuffManTree;

typedef struct HuffManNodeImpl HuffManNode;

typedef struct {
	HuffManNode* bit_0;
	HuffManNode* bit_1;
} HuffManNodeNode;

typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HuffManNodeTypeNode = 0,
	HuffManNodeTypeEnd,
	HuffManNodeTypeError
} HuffManNodeType;

struct HuffManNodeImpl {
	HuffManNodeType type;
	union {
		HuffManNodeNode node;
		uint8_t end;
		const char* error;
	} data;
};

struct HuffManTreeImpl {
	HuffManNode root;
    void* memory;
};

NODISCARD HuffManTree* get_hpack_huffman_tree(void);

void free_hpack_huffman_tree(HuffManTree* tree);

NODISCARD SizedBuffer apply_huffman_code(const HuffManTree* tree, SizedBuffer input);

void global_initialize_http2_hpack_huffman_data(void);

void global_free_http2_hpack_huffman_data(void);
