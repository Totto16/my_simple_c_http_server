

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/sized_buffer.h"
#include "utils/utils.h"

typedef struct HashSaltResultTypeImpl HashSaltResultType;

typedef struct {
	uint8_t work_factor; // between 4 and 31, defaults to  (12)
	bool use_sha512;     // default true
} HashSaltSettings;

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT_LIB

#if defined(_SIMPLE_SERVER_USE_BCRYPT_LIB_LIBBCRYPT)
#include <bcrypt.h>
#elif defined(_SIMPLE_SERVER_USE_BCRYPT_LIB_BCRYPT)
#error "bcrypt"
#define BCRYPT_DEFAULT_WORK_FACTOR TODO
#elif defined(_SIMPLE_SERVER_USE_BCRYPT_LIB_CRYPT_BLOWFISH)
#error "crypt_blowfish"
#define BCRYPT_DEFAULT_WORK_FACTOR TODO
#else
#error "Unrecognized bcrypt lib"
#endif

NODISCARD HashSaltResultType* hash_salt_string(HashSaltSettings settings, char* string);

NODISCARD bool is_string_equal_to_hash_salted_string(HashSaltSettings settings, char* string,
                                                     HashSaltResultType* hash_salted_string);

void free_hash_salted_result(HashSaltResultType* hash_salted_string);

#endif

NODISCARD SizedBuffer get_sha1_from_string(const char* string);

NODISCARD char* base64_encode_buffer(SizedBuffer input_buffer);

NODISCARD SizedBuffer base64_decode_buffer(SizedBuffer input_buffer);

NODISCARD const char* get_sha1_provider(void);

NODISCARD const char* get_base64_provider(void);

#ifdef _SIMPLE_SERVER_USE_OPENSSL

void openssl_initialize_crypto_thread_state(void);

void openssl_cleanup_crypto_thread_state(void);

#endif

#ifdef __cplusplus
}
#endif
