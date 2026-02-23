

#include <tmap.h>

#include "../http/protocol.h"

TMAP_HASH_FUNC_SIG(tstr, TSTR_KEYNAME) {
	return TMAP_HASH_BYTES(tstr_cstr(&key), tstr_len(&key));
}

TMAP_COMPARE_FUNC_SIG(tstr, TSTR_KEYNAME) {
	return tstr_cmp(&key1, &key2);
}
