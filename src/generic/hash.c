

#include "./hash.h"

NODISCARD HashSaltResultType hash_salt_string(HashSaltSettings settings, char* string) {

	UNUSED(settings);
	UNUSED(string);
	// TODO
	return get_empty_sized_buffer();
}

NODISCARD bool is_string_equal_to_hash_salted_string(HashSaltSettings settings, char* string,
                                                     HashSaltResultType hash_salted_string) {

	UNUSED(settings);
	UNUSED(string);
	UNUSED(hash_salted_string);

	// TODO
	return false;
}
