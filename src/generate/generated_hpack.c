
#include "http/hpack_huffman.h"

#define HUFFMAN_NODE_AMOUNT 513

NODISCARD HuffManTree* get_hpack_huffman_tree(void){

	HuffManTree* tree = malloc(sizeof(HuffManTree));

	if(tree == NULL) {
		return NULL;
	}

	HuffManNode* nodes_array = malloc(sizeof(HuffManNode) * HUFFMAN_NODE_AMOUNT);

	if(nodes_array == NULL) {
		free(tree);
		return NULL;
	}

	{
	
	nodes_array[0] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 470), .bit_1 = (nodes_array + 1) } } });
	nodes_array[1] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 435), .bit_1 = (nodes_array + 2) } } });
	nodes_array[2] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 404), .bit_1 = (nodes_array + 3) } } });
	nodes_array[3] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 389), .bit_1 = (nodes_array + 4) } } });
	nodes_array[4] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 382), .bit_1 = (nodes_array + 5) } } });
	nodes_array[5] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 375), .bit_1 = (nodes_array + 6) } } });
	nodes_array[6] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 372), .bit_1 = (nodes_array + 7) } } });
	nodes_array[7] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 365), .bit_1 = (nodes_array + 8) } } });
	nodes_array[8] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 360), .bit_1 = (nodes_array + 9) } } });
	nodes_array[9] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 355), .bit_1 = (nodes_array + 10) } } });
	nodes_array[10] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 348), .bit_1 = (nodes_array + 11) } } });
	nodes_array[11] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 345), .bit_1 = (nodes_array + 12) } } });
	nodes_array[12] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 342), .bit_1 = (nodes_array + 13) } } });
	nodes_array[13] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 339), .bit_1 = (nodes_array + 14) } } });
	nodes_array[14] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 338), .bit_1 = (nodes_array + 15) } } });
	nodes_array[15] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 309), .bit_1 = (nodes_array + 16) } } });
	nodes_array[16] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 264), .bit_1 = (nodes_array + 17) } } });
	nodes_array[17] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 225), .bit_1 = (nodes_array + 18) } } });
	nodes_array[18] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 194), .bit_1 = (nodes_array + 19) } } });
	nodes_array[19] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 173), .bit_1 = (nodes_array + 20) } } });
	nodes_array[20] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 154), .bit_1 = (nodes_array + 21) } } });
	nodes_array[21] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 121), .bit_1 = (nodes_array + 22) } } });
	nodes_array[22] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 90), .bit_1 = (nodes_array + 23) } } });
	nodes_array[23] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 61), .bit_1 = (nodes_array + 24) } } });
	nodes_array[24] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 46), .bit_1 = (nodes_array + 25) } } });
	nodes_array[25] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 39), .bit_1 = (nodes_array + 26) } } });
	nodes_array[26] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 36), .bit_1 = (nodes_array + 27) } } });
	nodes_array[27] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 35), .bit_1 = (nodes_array + 28) } } });
	nodes_array[28] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 32), .bit_1 = (nodes_array + 29) } } });
	nodes_array[29] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 31), .bit_1 = (nodes_array + 30) } } });
	nodes_array[30] = 
            ((HuffManNode){ .type = HuffManNodeTypeError, .data = { .error = "EOS received" } });
	nodes_array[31] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 22 } });
	nodes_array[32] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 34), .bit_1 = (nodes_array + 33) } } });
	nodes_array[33] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 13 } });
	nodes_array[34] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 10 } });
	nodes_array[35] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 249 } });
	nodes_array[36] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 38), .bit_1 = (nodes_array + 37) } } });
	nodes_array[37] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 220 } });
	nodes_array[38] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 127 } });
	nodes_array[39] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 43), .bit_1 = (nodes_array + 40) } } });
	nodes_array[40] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 42), .bit_1 = (nodes_array + 41) } } });
	nodes_array[41] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 31 } });
	nodes_array[42] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 30 } });
	nodes_array[43] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 45), .bit_1 = (nodes_array + 44) } } });
	nodes_array[44] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 29 } });
	nodes_array[45] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 28 } });
	nodes_array[46] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 54), .bit_1 = (nodes_array + 47) } } });
	nodes_array[47] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 51), .bit_1 = (nodes_array + 48) } } });
	nodes_array[48] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 50), .bit_1 = (nodes_array + 49) } } });
	nodes_array[49] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 27 } });
	nodes_array[50] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 26 } });
	nodes_array[51] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 53), .bit_1 = (nodes_array + 52) } } });
	nodes_array[52] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 25 } });
	nodes_array[53] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 24 } });
	nodes_array[54] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 58), .bit_1 = (nodes_array + 55) } } });
	nodes_array[55] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 57), .bit_1 = (nodes_array + 56) } } });
	nodes_array[56] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 23 } });
	nodes_array[57] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 21 } });
	nodes_array[58] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 60), .bit_1 = (nodes_array + 59) } } });
	nodes_array[59] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 20 } });
	nodes_array[60] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 19 } });
	nodes_array[61] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 77), .bit_1 = (nodes_array + 62) } } });
	nodes_array[62] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 70), .bit_1 = (nodes_array + 63) } } });
	nodes_array[63] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 67), .bit_1 = (nodes_array + 64) } } });
	nodes_array[64] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 66), .bit_1 = (nodes_array + 65) } } });
	nodes_array[65] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 18 } });
	nodes_array[66] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 17 } });
	nodes_array[67] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 69), .bit_1 = (nodes_array + 68) } } });
	nodes_array[68] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 16 } });
	nodes_array[69] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 15 } });
	nodes_array[70] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 74), .bit_1 = (nodes_array + 71) } } });
	nodes_array[71] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 73), .bit_1 = (nodes_array + 72) } } });
	nodes_array[72] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 14 } });
	nodes_array[73] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 12 } });
	nodes_array[74] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 76), .bit_1 = (nodes_array + 75) } } });
	nodes_array[75] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 11 } });
	nodes_array[76] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 8 } });
	nodes_array[77] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 85), .bit_1 = (nodes_array + 78) } } });
	nodes_array[78] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 82), .bit_1 = (nodes_array + 79) } } });
	nodes_array[79] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 81), .bit_1 = (nodes_array + 80) } } });
	nodes_array[80] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 7 } });
	nodes_array[81] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 6 } });
	nodes_array[82] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 84), .bit_1 = (nodes_array + 83) } } });
	nodes_array[83] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 5 } });
	nodes_array[84] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 4 } });
	nodes_array[85] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 89), .bit_1 = (nodes_array + 86) } } });
	nodes_array[86] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 88), .bit_1 = (nodes_array + 87) } } });
	nodes_array[87] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 3 } });
	nodes_array[88] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 2 } });
	nodes_array[89] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 254 } });
	nodes_array[90] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 106), .bit_1 = (nodes_array + 91) } } });
	nodes_array[91] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 99), .bit_1 = (nodes_array + 92) } } });
	nodes_array[92] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 96), .bit_1 = (nodes_array + 93) } } });
	nodes_array[93] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 95), .bit_1 = (nodes_array + 94) } } });
	nodes_array[94] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 253 } });
	nodes_array[95] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 252 } });
	nodes_array[96] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 98), .bit_1 = (nodes_array + 97) } } });
	nodes_array[97] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 251 } });
	nodes_array[98] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 250 } });
	nodes_array[99] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 103), .bit_1 = (nodes_array + 100) } } });
	nodes_array[100] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 102), .bit_1 = (nodes_array + 101) } } });
	nodes_array[101] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 248 } });
	nodes_array[102] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 247 } });
	nodes_array[103] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 105), .bit_1 = (nodes_array + 104) } } });
	nodes_array[104] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 246 } });
	nodes_array[105] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 245 } });
	nodes_array[106] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 114), .bit_1 = (nodes_array + 107) } } });
	nodes_array[107] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 111), .bit_1 = (nodes_array + 108) } } });
	nodes_array[108] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 110), .bit_1 = (nodes_array + 109) } } });
	nodes_array[109] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 244 } });
	nodes_array[110] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 241 } });
	nodes_array[111] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 113), .bit_1 = (nodes_array + 112) } } });
	nodes_array[112] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 223 } });
	nodes_array[113] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 222 } });
	nodes_array[114] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 118), .bit_1 = (nodes_array + 115) } } });
	nodes_array[115] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 117), .bit_1 = (nodes_array + 116) } } });
	nodes_array[116] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 221 } });
	nodes_array[117] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 214 } });
	nodes_array[118] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 120), .bit_1 = (nodes_array + 119) } } });
	nodes_array[119] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 212 } });
	nodes_array[120] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 211 } });
	nodes_array[121] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 139), .bit_1 = (nodes_array + 122) } } });
	nodes_array[122] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 132), .bit_1 = (nodes_array + 123) } } });
	nodes_array[123] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 129), .bit_1 = (nodes_array + 124) } } });
	nodes_array[124] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 128), .bit_1 = (nodes_array + 125) } } });
	nodes_array[125] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 127), .bit_1 = (nodes_array + 126) } } });
	nodes_array[126] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 204 } });
	nodes_array[127] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 203 } });
	nodes_array[128] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 255 } });
	nodes_array[129] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 131), .bit_1 = (nodes_array + 130) } } });
	nodes_array[130] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 243 } });
	nodes_array[131] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 242 } });
	nodes_array[132] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 136), .bit_1 = (nodes_array + 133) } } });
	nodes_array[133] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 135), .bit_1 = (nodes_array + 134) } } });
	nodes_array[134] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 240 } });
	nodes_array[135] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 238 } });
	nodes_array[136] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 138), .bit_1 = (nodes_array + 137) } } });
	nodes_array[137] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 219 } });
	nodes_array[138] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 218 } });
	nodes_array[139] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 147), .bit_1 = (nodes_array + 140) } } });
	nodes_array[140] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 144), .bit_1 = (nodes_array + 141) } } });
	nodes_array[141] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 143), .bit_1 = (nodes_array + 142) } } });
	nodes_array[142] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 213 } });
	nodes_array[143] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 210 } });
	nodes_array[144] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 146), .bit_1 = (nodes_array + 145) } } });
	nodes_array[145] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 205 } });
	nodes_array[146] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 202 } });
	nodes_array[147] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 151), .bit_1 = (nodes_array + 148) } } });
	nodes_array[148] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 150), .bit_1 = (nodes_array + 149) } } });
	nodes_array[149] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 201 } });
	nodes_array[150] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 200 } });
	nodes_array[151] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 153), .bit_1 = (nodes_array + 152) } } });
	nodes_array[152] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 193 } });
	nodes_array[153] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 192 } });
	nodes_array[154] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 166), .bit_1 = (nodes_array + 155) } } });
	nodes_array[155] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 163), .bit_1 = (nodes_array + 156) } } });
	nodes_array[156] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 160), .bit_1 = (nodes_array + 157) } } });
	nodes_array[157] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 159), .bit_1 = (nodes_array + 158) } } });
	nodes_array[158] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 235 } });
	nodes_array[159] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 234 } });
	nodes_array[160] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 162), .bit_1 = (nodes_array + 161) } } });
	nodes_array[161] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 207 } });
	nodes_array[162] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 199 } });
	nodes_array[163] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 165), .bit_1 = (nodes_array + 164) } } });
	nodes_array[164] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 237 } });
	nodes_array[165] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 236 } });
	nodes_array[166] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 170), .bit_1 = (nodes_array + 167) } } });
	nodes_array[167] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 169), .bit_1 = (nodes_array + 168) } } });
	nodes_array[168] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 225 } });
	nodes_array[169] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 215 } });
	nodes_array[170] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 172), .bit_1 = (nodes_array + 171) } } });
	nodes_array[171] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 206 } });
	nodes_array[172] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 171 } });
	nodes_array[173] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 187), .bit_1 = (nodes_array + 174) } } });
	nodes_array[174] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 182), .bit_1 = (nodes_array + 175) } } });
	nodes_array[175] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 179), .bit_1 = (nodes_array + 176) } } });
	nodes_array[176] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 178), .bit_1 = (nodes_array + 177) } } });
	nodes_array[177] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 159 } });
	nodes_array[178] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 148 } });
	nodes_array[179] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 181), .bit_1 = (nodes_array + 180) } } });
	nodes_array[180] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 145 } });
	nodes_array[181] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 144 } });
	nodes_array[182] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 186), .bit_1 = (nodes_array + 183) } } });
	nodes_array[183] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 185), .bit_1 = (nodes_array + 184) } } });
	nodes_array[184] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 142 } });
	nodes_array[185] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 9 } });
	nodes_array[186] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 239 } });
	nodes_array[187] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 191), .bit_1 = (nodes_array + 188) } } });
	nodes_array[188] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 190), .bit_1 = (nodes_array + 189) } } });
	nodes_array[189] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 231 } });
	nodes_array[190] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 197 } });
	nodes_array[191] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 193), .bit_1 = (nodes_array + 192) } } });
	nodes_array[192] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 191 } });
	nodes_array[193] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 188 } });
	nodes_array[194] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 210), .bit_1 = (nodes_array + 195) } } });
	nodes_array[195] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 203), .bit_1 = (nodes_array + 196) } } });
	nodes_array[196] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 200), .bit_1 = (nodes_array + 197) } } });
	nodes_array[197] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 199), .bit_1 = (nodes_array + 198) } } });
	nodes_array[198] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 183 } });
	nodes_array[199] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 182 } });
	nodes_array[200] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 202), .bit_1 = (nodes_array + 201) } } });
	nodes_array[201] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 180 } });
	nodes_array[202] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 175 } });
	nodes_array[203] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 207), .bit_1 = (nodes_array + 204) } } });
	nodes_array[204] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 206), .bit_1 = (nodes_array + 205) } } });
	nodes_array[205] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 174 } });
	nodes_array[206] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 168 } });
	nodes_array[207] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 209), .bit_1 = (nodes_array + 208) } } });
	nodes_array[208] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 166 } });
	nodes_array[209] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 165 } });
	nodes_array[210] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 218), .bit_1 = (nodes_array + 211) } } });
	nodes_array[211] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 215), .bit_1 = (nodes_array + 212) } } });
	nodes_array[212] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 214), .bit_1 = (nodes_array + 213) } } });
	nodes_array[213] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 158 } });
	nodes_array[214] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 157 } });
	nodes_array[215] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 217), .bit_1 = (nodes_array + 216) } } });
	nodes_array[216] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 155 } });
	nodes_array[217] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 152 } });
	nodes_array[218] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 222), .bit_1 = (nodes_array + 219) } } });
	nodes_array[219] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 221), .bit_1 = (nodes_array + 220) } } });
	nodes_array[220] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 151 } });
	nodes_array[221] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 150 } });
	nodes_array[222] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 224), .bit_1 = (nodes_array + 223) } } });
	nodes_array[223] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 149 } });
	nodes_array[224] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 147 } });
	nodes_array[225] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 249), .bit_1 = (nodes_array + 226) } } });
	nodes_array[226] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 242), .bit_1 = (nodes_array + 227) } } });
	nodes_array[227] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 235), .bit_1 = (nodes_array + 228) } } });
	nodes_array[228] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 232), .bit_1 = (nodes_array + 229) } } });
	nodes_array[229] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 231), .bit_1 = (nodes_array + 230) } } });
	nodes_array[230] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 143 } });
	nodes_array[231] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 141 } });
	nodes_array[232] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 234), .bit_1 = (nodes_array + 233) } } });
	nodes_array[233] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 140 } });
	nodes_array[234] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 139 } });
	nodes_array[235] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 239), .bit_1 = (nodes_array + 236) } } });
	nodes_array[236] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 238), .bit_1 = (nodes_array + 237) } } });
	nodes_array[237] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 138 } });
	nodes_array[238] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 137 } });
	nodes_array[239] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 241), .bit_1 = (nodes_array + 240) } } });
	nodes_array[240] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 135 } });
	nodes_array[241] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 1 } });
	nodes_array[242] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 246), .bit_1 = (nodes_array + 243) } } });
	nodes_array[243] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 245), .bit_1 = (nodes_array + 244) } } });
	nodes_array[244] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 233 } });
	nodes_array[245] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 232 } });
	nodes_array[246] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 248), .bit_1 = (nodes_array + 247) } } });
	nodes_array[247] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 228 } });
	nodes_array[248] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 198 } });
	nodes_array[249] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 257), .bit_1 = (nodes_array + 250) } } });
	nodes_array[250] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 254), .bit_1 = (nodes_array + 251) } } });
	nodes_array[251] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 253), .bit_1 = (nodes_array + 252) } } });
	nodes_array[252] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 196 } });
	nodes_array[253] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 190 } });
	nodes_array[254] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 256), .bit_1 = (nodes_array + 255) } } });
	nodes_array[255] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 189 } });
	nodes_array[256] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 187 } });
	nodes_array[257] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 261), .bit_1 = (nodes_array + 258) } } });
	nodes_array[258] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 260), .bit_1 = (nodes_array + 259) } } });
	nodes_array[259] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 186 } });
	nodes_array[260] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 185 } });
	nodes_array[261] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 263), .bit_1 = (nodes_array + 262) } } });
	nodes_array[262] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 181 } });
	nodes_array[263] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 178 } });
	nodes_array[264] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 294), .bit_1 = (nodes_array + 265) } } });
	nodes_array[265] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 281), .bit_1 = (nodes_array + 266) } } });
	nodes_array[266] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 274), .bit_1 = (nodes_array + 267) } } });
	nodes_array[267] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 271), .bit_1 = (nodes_array + 268) } } });
	nodes_array[268] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 270), .bit_1 = (nodes_array + 269) } } });
	nodes_array[269] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 173 } });
	nodes_array[270] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 170 } });
	nodes_array[271] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 273), .bit_1 = (nodes_array + 272) } } });
	nodes_array[272] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 169 } });
	nodes_array[273] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 164 } });
	nodes_array[274] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 278), .bit_1 = (nodes_array + 275) } } });
	nodes_array[275] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 277), .bit_1 = (nodes_array + 276) } } });
	nodes_array[276] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 163 } });
	nodes_array[277] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 160 } });
	nodes_array[278] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 280), .bit_1 = (nodes_array + 279) } } });
	nodes_array[279] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 156 } });
	nodes_array[280] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 154 } });
	nodes_array[281] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 289), .bit_1 = (nodes_array + 282) } } });
	nodes_array[282] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 286), .bit_1 = (nodes_array + 283) } } });
	nodes_array[283] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 285), .bit_1 = (nodes_array + 284) } } });
	nodes_array[284] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 146 } });
	nodes_array[285] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 136 } });
	nodes_array[286] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 288), .bit_1 = (nodes_array + 287) } } });
	nodes_array[287] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 134 } });
	nodes_array[288] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 133 } });
	nodes_array[289] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 293), .bit_1 = (nodes_array + 290) } } });
	nodes_array[290] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 292), .bit_1 = (nodes_array + 291) } } });
	nodes_array[291] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 132 } });
	nodes_array[292] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 129 } });
	nodes_array[293] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 230 } });
	nodes_array[294] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 302), .bit_1 = (nodes_array + 295) } } });
	nodes_array[295] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 299), .bit_1 = (nodes_array + 296) } } });
	nodes_array[296] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 298), .bit_1 = (nodes_array + 297) } } });
	nodes_array[297] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 229 } });
	nodes_array[298] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 227 } });
	nodes_array[299] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 301), .bit_1 = (nodes_array + 300) } } });
	nodes_array[300] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 217 } });
	nodes_array[301] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 216 } });
	nodes_array[302] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 306), .bit_1 = (nodes_array + 303) } } });
	nodes_array[303] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 305), .bit_1 = (nodes_array + 304) } } });
	nodes_array[304] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 209 } });
	nodes_array[305] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 179 } });
	nodes_array[306] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 308), .bit_1 = (nodes_array + 307) } } });
	nodes_array[307] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 177 } });
	nodes_array[308] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 176 } });
	nodes_array[309] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 329), .bit_1 = (nodes_array + 310) } } });
	nodes_array[310] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 322), .bit_1 = (nodes_array + 311) } } });
	nodes_array[311] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 319), .bit_1 = (nodes_array + 312) } } });
	nodes_array[312] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 316), .bit_1 = (nodes_array + 313) } } });
	nodes_array[313] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 315), .bit_1 = (nodes_array + 314) } } });
	nodes_array[314] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 172 } });
	nodes_array[315] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 167 } });
	nodes_array[316] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 318), .bit_1 = (nodes_array + 317) } } });
	nodes_array[317] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 161 } });
	nodes_array[318] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 153 } });
	nodes_array[319] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 321), .bit_1 = (nodes_array + 320) } } });
	nodes_array[320] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 226 } });
	nodes_array[321] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 224 } });
	nodes_array[322] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 326), .bit_1 = (nodes_array + 323) } } });
	nodes_array[323] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 325), .bit_1 = (nodes_array + 324) } } });
	nodes_array[324] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 194 } });
	nodes_array[325] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 184 } });
	nodes_array[326] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 328), .bit_1 = (nodes_array + 327) } } });
	nodes_array[327] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 162 } });
	nodes_array[328] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 131 } });
	nodes_array[329] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 335), .bit_1 = (nodes_array + 330) } } });
	nodes_array[330] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 334), .bit_1 = (nodes_array + 331) } } });
	nodes_array[331] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 333), .bit_1 = (nodes_array + 332) } } });
	nodes_array[332] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 130 } });
	nodes_array[333] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 128 } });
	nodes_array[334] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 208 } });
	nodes_array[335] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 337), .bit_1 = (nodes_array + 336) } } });
	nodes_array[336] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 195 } });
	nodes_array[337] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 92 } });
	nodes_array[338] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 123 } });
	nodes_array[339] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 341), .bit_1 = (nodes_array + 340) } } });
	nodes_array[340] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 96 } });
	nodes_array[341] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 60 } });
	nodes_array[342] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 344), .bit_1 = (nodes_array + 343) } } });
	nodes_array[343] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 125 } });
	nodes_array[344] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 94 } });
	nodes_array[345] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 347), .bit_1 = (nodes_array + 346) } } });
	nodes_array[346] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 126 } });
	nodes_array[347] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 93 } });
	nodes_array[348] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 352), .bit_1 = (nodes_array + 349) } } });
	nodes_array[349] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 351), .bit_1 = (nodes_array + 350) } } });
	nodes_array[350] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 91 } });
	nodes_array[351] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 64 } });
	nodes_array[352] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 354), .bit_1 = (nodes_array + 353) } } });
	nodes_array[353] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 36 } });
	nodes_array[354] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 0 } });
	nodes_array[355] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 359), .bit_1 = (nodes_array + 356) } } });
	nodes_array[356] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 358), .bit_1 = (nodes_array + 357) } } });
	nodes_array[357] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 62 } });
	nodes_array[358] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 35 } });
	nodes_array[359] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 124 } });
	nodes_array[360] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 364), .bit_1 = (nodes_array + 361) } } });
	nodes_array[361] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 363), .bit_1 = (nodes_array + 362) } } });
	nodes_array[362] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 43 } });
	nodes_array[363] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 39 } });
	nodes_array[364] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 63 } });
	nodes_array[365] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 369), .bit_1 = (nodes_array + 366) } } });
	nodes_array[366] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 368), .bit_1 = (nodes_array + 367) } } });
	nodes_array[367] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 41 } });
	nodes_array[368] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 40 } });
	nodes_array[369] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 371), .bit_1 = (nodes_array + 370) } } });
	nodes_array[370] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 34 } });
	nodes_array[371] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 33 } });
	nodes_array[372] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 374), .bit_1 = (nodes_array + 373) } } });
	nodes_array[373] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 90 } });
	nodes_array[374] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 88 } });
	nodes_array[375] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 379), .bit_1 = (nodes_array + 376) } } });
	nodes_array[376] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 378), .bit_1 = (nodes_array + 377) } } });
	nodes_array[377] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 59 } });
	nodes_array[378] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 44 } });
	nodes_array[379] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 381), .bit_1 = (nodes_array + 380) } } });
	nodes_array[380] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 42 } });
	nodes_array[381] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 38 } });
	nodes_array[382] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 386), .bit_1 = (nodes_array + 383) } } });
	nodes_array[383] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 385), .bit_1 = (nodes_array + 384) } } });
	nodes_array[384] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 122 } });
	nodes_array[385] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 121 } });
	nodes_array[386] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 388), .bit_1 = (nodes_array + 387) } } });
	nodes_array[387] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 120 } });
	nodes_array[388] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 119 } });
	nodes_array[389] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 397), .bit_1 = (nodes_array + 390) } } });
	nodes_array[390] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 394), .bit_1 = (nodes_array + 391) } } });
	nodes_array[391] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 393), .bit_1 = (nodes_array + 392) } } });
	nodes_array[392] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 118 } });
	nodes_array[393] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 113 } });
	nodes_array[394] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 396), .bit_1 = (nodes_array + 395) } } });
	nodes_array[395] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 107 } });
	nodes_array[396] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 106 } });
	nodes_array[397] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 401), .bit_1 = (nodes_array + 398) } } });
	nodes_array[398] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 400), .bit_1 = (nodes_array + 399) } } });
	nodes_array[399] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 89 } });
	nodes_array[400] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 87 } });
	nodes_array[401] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 403), .bit_1 = (nodes_array + 402) } } });
	nodes_array[402] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 86 } });
	nodes_array[403] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 85 } });
	nodes_array[404] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 420), .bit_1 = (nodes_array + 405) } } });
	nodes_array[405] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 413), .bit_1 = (nodes_array + 406) } } });
	nodes_array[406] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 410), .bit_1 = (nodes_array + 407) } } });
	nodes_array[407] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 409), .bit_1 = (nodes_array + 408) } } });
	nodes_array[408] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 84 } });
	nodes_array[409] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 83 } });
	nodes_array[410] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 412), .bit_1 = (nodes_array + 411) } } });
	nodes_array[411] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 82 } });
	nodes_array[412] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 81 } });
	nodes_array[413] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 417), .bit_1 = (nodes_array + 414) } } });
	nodes_array[414] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 416), .bit_1 = (nodes_array + 415) } } });
	nodes_array[415] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 80 } });
	nodes_array[416] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 79 } });
	nodes_array[417] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 419), .bit_1 = (nodes_array + 418) } } });
	nodes_array[418] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 78 } });
	nodes_array[419] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 77 } });
	nodes_array[420] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 428), .bit_1 = (nodes_array + 421) } } });
	nodes_array[421] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 425), .bit_1 = (nodes_array + 422) } } });
	nodes_array[422] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 424), .bit_1 = (nodes_array + 423) } } });
	nodes_array[423] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 76 } });
	nodes_array[424] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 75 } });
	nodes_array[425] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 427), .bit_1 = (nodes_array + 426) } } });
	nodes_array[426] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 74 } });
	nodes_array[427] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 73 } });
	nodes_array[428] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 432), .bit_1 = (nodes_array + 429) } } });
	nodes_array[429] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 431), .bit_1 = (nodes_array + 430) } } });
	nodes_array[430] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 72 } });
	nodes_array[431] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 71 } });
	nodes_array[432] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 434), .bit_1 = (nodes_array + 433) } } });
	nodes_array[433] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 70 } });
	nodes_array[434] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 69 } });
	nodes_array[435] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 455), .bit_1 = (nodes_array + 436) } } });
	nodes_array[436] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 448), .bit_1 = (nodes_array + 437) } } });
	nodes_array[437] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 445), .bit_1 = (nodes_array + 438) } } });
	nodes_array[438] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 442), .bit_1 = (nodes_array + 439) } } });
	nodes_array[439] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 441), .bit_1 = (nodes_array + 440) } } });
	nodes_array[440] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 68 } });
	nodes_array[441] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 67 } });
	nodes_array[442] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 444), .bit_1 = (nodes_array + 443) } } });
	nodes_array[443] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 66 } });
	nodes_array[444] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 58 } });
	nodes_array[445] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 447), .bit_1 = (nodes_array + 446) } } });
	nodes_array[446] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 117 } });
	nodes_array[447] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 114 } });
	nodes_array[448] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 452), .bit_1 = (nodes_array + 449) } } });
	nodes_array[449] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 451), .bit_1 = (nodes_array + 450) } } });
	nodes_array[450] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 112 } });
	nodes_array[451] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 110 } });
	nodes_array[452] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 454), .bit_1 = (nodes_array + 453) } } });
	nodes_array[453] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 109 } });
	nodes_array[454] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 108 } });
	nodes_array[455] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 463), .bit_1 = (nodes_array + 456) } } });
	nodes_array[456] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 460), .bit_1 = (nodes_array + 457) } } });
	nodes_array[457] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 459), .bit_1 = (nodes_array + 458) } } });
	nodes_array[458] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 104 } });
	nodes_array[459] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 103 } });
	nodes_array[460] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 462), .bit_1 = (nodes_array + 461) } } });
	nodes_array[461] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 102 } });
	nodes_array[462] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 100 } });
	nodes_array[463] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 467), .bit_1 = (nodes_array + 464) } } });
	nodes_array[464] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 466), .bit_1 = (nodes_array + 465) } } });
	nodes_array[465] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 98 } });
	nodes_array[466] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 95 } });
	nodes_array[467] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 469), .bit_1 = (nodes_array + 468) } } });
	nodes_array[468] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 65 } });
	nodes_array[469] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 61 } });
	nodes_array[470] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 498), .bit_1 = (nodes_array + 471) } } });
	nodes_array[471] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 487), .bit_1 = (nodes_array + 472) } } });
	nodes_array[472] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 480), .bit_1 = (nodes_array + 473) } } });
	nodes_array[473] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 477), .bit_1 = (nodes_array + 474) } } });
	nodes_array[474] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 476), .bit_1 = (nodes_array + 475) } } });
	nodes_array[475] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 57 } });
	nodes_array[476] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 56 } });
	nodes_array[477] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 479), .bit_1 = (nodes_array + 478) } } });
	nodes_array[478] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 55 } });
	nodes_array[479] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 54 } });
	nodes_array[480] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 484), .bit_1 = (nodes_array + 481) } } });
	nodes_array[481] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 483), .bit_1 = (nodes_array + 482) } } });
	nodes_array[482] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 53 } });
	nodes_array[483] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 52 } });
	nodes_array[484] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 486), .bit_1 = (nodes_array + 485) } } });
	nodes_array[485] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 51 } });
	nodes_array[486] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 47 } });
	nodes_array[487] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 495), .bit_1 = (nodes_array + 488) } } });
	nodes_array[488] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 492), .bit_1 = (nodes_array + 489) } } });
	nodes_array[489] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 491), .bit_1 = (nodes_array + 490) } } });
	nodes_array[490] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 46 } });
	nodes_array[491] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 45 } });
	nodes_array[492] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 494), .bit_1 = (nodes_array + 493) } } });
	nodes_array[493] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 37 } });
	nodes_array[494] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 32 } });
	nodes_array[495] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 497), .bit_1 = (nodes_array + 496) } } });
	nodes_array[496] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 116 } });
	nodes_array[497] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 115 } });
	nodes_array[498] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 506), .bit_1 = (nodes_array + 499) } } });
	nodes_array[499] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 503), .bit_1 = (nodes_array + 500) } } });
	nodes_array[500] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 502), .bit_1 = (nodes_array + 501) } } });
	nodes_array[501] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 111 } });
	nodes_array[502] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 105 } });
	nodes_array[503] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 505), .bit_1 = (nodes_array + 504) } } });
	nodes_array[504] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 101 } });
	nodes_array[505] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 99 } });
	nodes_array[506] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 510), .bit_1 = (nodes_array + 507) } } });
	nodes_array[507] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 509), .bit_1 = (nodes_array + 508) } } });
	nodes_array[508] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 97 } });
	nodes_array[509] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 50 } });
	nodes_array[510] = 
            ((HuffManNode){ .type = HuffManNodeTypeNode, .data = { .node = (HuffManNodeNode){ .bit_0 = (nodes_array + 512), .bit_1 = (nodes_array + 511) } } });
	nodes_array[511] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 49 } });
	nodes_array[512] = 
            ((HuffManNode){ .type = HuffManNodeTypeEnd, .data = { .end = 48 } });

	}


	*tree = (HuffManNode){ .root = root, .memory = (void*)nodes_array };

	return tree;
}

void free_hpack_huffman_tree(HuffManTree* tree){
	free(tree->memory);
	free(tree);
}
