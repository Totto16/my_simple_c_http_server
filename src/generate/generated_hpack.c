
#include "./generated_hpack.h"

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
};

NODISCARD HuffManTree* get_hpack_huffman_tree(void){

    HuffManTree* tree = malloc(sizeof(HuffManTree)),

    if(tree == NULL){
        return NULL;
    }

    *tree = (HuffManNode){.root = root};

    return tree;
//TODO
}

NODISCARD SizedBuffer apply_huffman_code(const HuffManTree* const tree, const SizedBuffer input){

//TODO
}

