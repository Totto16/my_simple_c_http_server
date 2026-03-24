#include <tmap.h>

#include "../http/mime.h"
#include "../http/protocol.h"

TMAP_HASH_FUNC_SIG(tstr, TSTR_KEYNAME) {
	return TMAP_HASH_BYTES(tstr_cstr(&key), tstr_len(&key));
}

TMAP_EQ_FUNC_SIG(tstr, TSTR_KEYNAME) {
	return tstr_eq(&key1, &key2);
}

TMAP_HASH_FUNC_SIG(tstr_view, TStringView) {
	return TMAP_HASH_BYTES(key.data, key.len);
}

TMAP_EQ_FUNC_SIG(tstr_view, TStringView) {
	return tstr_view_eq_view(key1, key2);
}
