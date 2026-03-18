#include <tmap.h>

#include "../http/mime.h"
#include "../http/protocol.h"

TMAP_HASH_FUNC_SIG(tstr, TSTR_KEYNAME) {
	return TMAP_HASH_BYTES(tstr_cstr(&key), tstr_len(&key));
}

TMAP_COMPARE_FUNC_SIG(tstr, TSTR_KEYNAME) {
	return tstr_cmp(&key1, &key2);
}

TMAP_HASH_FUNC_SIG(tstr_view, TStringView) {
	return TMAP_HASH_BYTES(key.data, key.len);
}

TMAP_COMPARE_FUNC_SIG(tstr_view, TStringView) {
	return tstr_view_cmp(key1, key2);
}
