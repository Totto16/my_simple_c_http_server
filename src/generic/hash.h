

#pragma once

#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct {
	size_t todo; // use openssl or libcrypt
} HashSaltSettings;

typedef SizedBuffer HashSaltResultType;

NODISCARD HashSaltResultType hash_salt_string(HashSaltSettings settings, char* string);

NODISCARD bool is_string_equal_to_hash_salted_string(HashSaltSettings settings, char* string,
                                                     HashSaltResultType hash_salted_string);

NODISCARD SizedBuffer get_sha1_from_string(const char* string);

NODISCARD char* base64_encode_buffer(SizedBuffer input_buffer);

NODISCARD SizedBuffer base64_decode_buffer(SizedBuffer input_buffer);
