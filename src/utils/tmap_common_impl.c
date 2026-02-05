

#include <tmap.h>

#include "../http/protocol.h"

TMAP_HASH_FUNC_SIG(char*, CHAR_PTR_KEYNAME) {
	return TMAP_HASH_STR(key);
}

TMAP_COMPARE_FUNC_SIG(char*, CHAR_PTR_KEYNAME) {
	return strcmp(key1, key2);
}
