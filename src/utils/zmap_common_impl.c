

#include <zmap/zmap.h>

#include "./types.h"

ZMAP_HASH_FUNC_SIG(char*, CHAR_PTR_KEYNAME) {
	return ZMAP_HASH_STR(key);
}

ZMAP_COMPARE_FUNC_SIG(char*, CHAR_PTR_KEYNAME) {
	return strcmp(key1, key2);
}
