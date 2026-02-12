
#pragma once

#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct HuffManTreeImpl HuffManTree;

NODISCARD HuffManTree* get_hpack_huffman_tree(void);

NODISCARD SizedBuffer apply_huffman_code(const HuffManTree* tree, SizedBuffer input);


