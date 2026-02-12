
#include "./hpack_huffman.h"

NODISCARD SizedBuffer apply_huffman_code(const HuffManTree* const tree, SizedBuffer input) {
	// TODO
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
