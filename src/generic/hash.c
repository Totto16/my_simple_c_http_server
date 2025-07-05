

#include "./hash.h"
#include "utils/log.h"

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT
#include <bcrypt.h>
#endif

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT

struct HashSaltResultTypeImpl {
	char hash[BCRYPT_HASHSIZE];
};

NODISCARD HashSaltResultType* hash_salt_string(HashSaltSettings settings, char* string) {

	char* password_to_use = string;

	if(settings.use_sha512) {

		char result_digest[BCRYPT_512BITS_BASE64_SIZE] = {};

		int res = bcrypt_sha512(string, result_digest);

		if(res != 0) {
			return NULL;
		}

		char* new_char = malloc(BCRYPT_512BITS_BASE64_SIZE + 1);

		if(!new_char) {
			return NULL;
		}

		new_char[BCRYPT_512BITS_BASE64_SIZE] = '\0';

		memcpy(new_char, &result_digest, BCRYPT_512BITS_BASE64_SIZE);

		password_to_use = new_char;
	}

	char result_salt[BCRYPT_HASHSIZE] = {};

	int res = bcrypt_gensalt(settings.work_factor, result_salt);

	if(res != 0) {
		if(settings.use_sha512) {
			free(password_to_use);
		}
		return NULL;
	}

	char result_hash[BCRYPT_HASHSIZE] = {};

	res = bcrypt_hashpw(password_to_use, result_salt, result_hash);

	if(settings.use_sha512) {
		free(password_to_use);
	}

	if(res != 0) {
		return NULL;
	}

	HashSaltResultType* result_type = malloc(sizeof(HashSaltResultType));

	if(!result_type) {
		return NULL;
	}

	memcpy(&result_type->hash, &result_hash, BCRYPT_HASHSIZE);

	return result_type;
}

NODISCARD bool is_string_equal_to_hash_salted_string(HashSaltSettings settings, char* string,
                                                     HashSaltResultType* hash_salted_string) {

	char* password_to_use = string;

	if(settings.use_sha512) {

		char input_digest[BCRYPT_512BITS_BASE64_SIZE] = {};

		int res = bcrypt_sha512(string, input_digest);

		if(res != 0) {
			return NULL;
		}

		char* new_char = malloc(BCRYPT_512BITS_BASE64_SIZE + 1);

		if(!new_char) {
			return NULL;
		}

		new_char[BCRYPT_512BITS_BASE64_SIZE] = '\0';

		memcpy(new_char, &input_digest, BCRYPT_512BITS_BASE64_SIZE);

		password_to_use = new_char;
	}

	int res = bcrypt_checkpw(password_to_use, hash_salted_string->hash);

	if(settings.use_sha512) {
		free(password_to_use);
	}

	if(res < 0) {
		return false;
	}

	return res == 0;
}

void free_hash_salted_result(HashSaltResultType* hash_salted_string) {
	free(hash_salted_string);
}

#endif

#ifdef _SIMPLE_SERVER_USE_OPENSSL_FOR_HASHING

// see: https://docs.openssl.org/3.5/man3/

#ifdef _SIMPLE_SERVER_USE_DEPRECATED_OPENSSL_SHA_FUNCTIONS

#include <openssl/sha.h>

SizedBuffer get_sha1_from_string(const char* string) {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	SHA_CTX sha_context;

	int result = SHA1_Init(&sha_context);

	if(result != 1) {
		return get_empty_sized_buffer();
	}

	result = SHA1_Update(&sha_context, (uint8_t*)string, strlen(string));

	if(result != 1) {
		return get_empty_sized_buffer();
	}

	uint8_t* sha1_result = malloc(SHA_DIGEST_LENGTH * sizeof(uint8_t));

	if(!sha1_result) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return get_empty_sized_buffer();
	}

	result = SHA1_Final(sha1_result, &sha_context);

#pragma GCC diagnostic pop

	if(result != 1) {
		return get_empty_sized_buffer();
	}

	return (SizedBuffer){ .data = sha1_result, .size = SHA_DIGEST_LENGTH };
}

#else

#include <openssl/evp.h>

SizedBuffer get_sha1_from_string(const char* string) {

	EVP_MD_CTX* evp_context = EVP_MD_CTX_new();

	int result = EVP_DigestInit_ex(evp_context, EVP_sha1(), NULL);

	if(result != 1) {
		EVP_MD_CTX_free(evp_context);
		return get_empty_sized_buffer();
	}

	result = EVP_DigestUpdate(evp_context, (uint8_t*)string, strlen(string));

	if(result != 1) {
		EVP_MD_CTX_free(evp_context);
		return get_empty_sized_buffer();
	}

	uint8_t* sha1_result = malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));

	if(!sha1_result) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return get_empty_sized_buffer();
	}

	unsigned int hash_len = 0;

	result = EVP_DigestFinal_ex(evp_context, sha1_result, &hash_len);

	if(result != 1) {
		free(sha1_result);
		EVP_MD_CTX_free(evp_context);
		return get_empty_sized_buffer();
	}

	EVP_MD_CTX_free(evp_context);

	return (SizedBuffer){ .data = sha1_result, .size = hash_len };
}

#endif

#else

#define SHA1_LEN 20

#include <sha1/sha1.h>

SizedBuffer get_sha1_from_string(const char* string) {

	SHA1_CTX sha_context;

	SHA1Init(&sha_context);

	SHA1Update(&sha_context, (uint8_t*)string, strlen(string));

	uint8_t* sha1_result = malloc(SHA1_LEN * sizeof(uint8_t));

	if(!sha1_result) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return get_empty_sized_buffer();
	}

	SHA1Final(sha1_result, &sha_context);

	return (SizedBuffer){ .data = sha1_result, .size = SHA1_LEN };
}

#endif

#ifdef _SIMPLE_SERVER_USE_OPENSSL_FOR_HASHING

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h> //NOLINT(readability-duplicate-include)

NODISCARD char* base64_encode_buffer(SizedBuffer input_buffer) {

	BIO* mem_input = NULL;
	BIO* b64_filter = NULL;
	BUF_MEM* buffer_ptr = NULL;

	b64_filter = BIO_new(BIO_f_base64());
	mem_input = BIO_new(BIO_s_mem());
	mem_input = BIO_push(b64_filter, mem_input);

	// Ignore newlines, write everything in one line
	BIO_set_flags(mem_input, BIO_FLAGS_BASE64_NO_NL);

	BIO_write(mem_input, input_buffer.data, (int)input_buffer.size);
	BIO_flush(mem_input);
	BIO_get_mem_ptr(mem_input, &buffer_ptr);

	size_t lenght = buffer_ptr->length * sizeof(char);

	char* result = (char*)malloc(lenght + 1);
	memcpy(result, buffer_ptr->data, lenght);
	result[lenght] = '\0';

	BIO_free_all(mem_input);
	return result;
}

#define B64_CHUNCK_SIZE 512

NODISCARD SizedBuffer base64_decode_buffer(SizedBuffer input_buffer) {
	if(input_buffer.size == 0) {

		char* empty_str = malloc(1);

		empty_str[0] = '\0';

		return (SizedBuffer){ .data = empty_str, .size = 0 };
	}

	BIO* mem_input = NULL;
	BIO* b64_filter = NULL;

	uint8_t* output_buffer_current = malloc(B64_CHUNCK_SIZE);
	SizedBuffer output_buffer = { .data = output_buffer_current, .size = 0 };

	b64_filter = BIO_new(BIO_f_base64());
	mem_input = BIO_new_mem_buf(input_buffer.data, (int)input_buffer.size);
	mem_input = BIO_push(b64_filter, mem_input);

	// Ignore newlines, when reading, (even if there are none)
	BIO_set_flags(mem_input, BIO_FLAGS_BASE64_NO_NL);
	while(true) {
		size_t read_size = 0;
		int result = BIO_read_ex(mem_input, output_buffer_current, B64_CHUNCK_SIZE, &read_size);

		if(result != 1) {
			BIO_free_all(mem_input);
			free_sized_buffer(output_buffer);
			return get_empty_sized_buffer();
		}

		output_buffer.size += read_size;

		// we need to perform more reads
		if(read_size == B64_CHUNCK_SIZE) {
			void* new_chunk = realloc(output_buffer.data, output_buffer.size + B64_CHUNCK_SIZE);
			output_buffer.data = new_chunk;
			output_buffer_current = (uint8_t*)output_buffer.data + output_buffer.size;
			continue;
		}

		// the read returned 0 or < B64_CHUNCK_SIZE, so we read until the end
		break;
	}

	BIO_free_all(mem_input);
	return output_buffer;
}

#else

#include <b64/b64.h>

NODISCARD char* base64_encode_buffer(SizedBuffer input_buffer) {
	return b64_encode(input_buffer.data, input_buffer.size);
}

NODISCARD SizedBuffer base64_decode_buffer(SizedBuffer input_buffer) {
	size_t result_size = 0;
	uint8_t* result = b64_decode_ex(input_buffer.data, input_buffer.size, &result_size);

	return (SizedBuffer){ .data = result, .size = result_size };
}

#endif

NODISCARD const char* get_sha1_provider(void) {
#ifdef _SIMPLE_SERVER_USE_OPENSSL_FOR_HASHING
#ifdef _SIMPLE_SERVER_USE_DEPRECATED_OPENSSL_SHA_FUNCTIONS

	return "openssl (Deprecated)";
#else
	return "openssl (EVP)";
#endif
#else
	return "thirdparty";
#endif
}

NODISCARD const char* get_base64_provider(void) {
#ifdef _SIMPLE_SERVER_USE_OPENSSL_FOR_HASHING
	return "openssl";
#else
	return "thirdparty";
#endif
}

#ifdef _SIMPLE_SERVER_USE_OPENSSL

#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

void openssl_initialize_crypto_thread_state(void) {
	uint64_t options = 0;

	int res = OPENSSL_init_crypto(options, NULL);

	if(res != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Failed to setup OPENSSL crypto thread state\n");
	}
}

void openssl_cleanup_crypto_thread_state(void) {
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();
	CONF_modules_unload(1);

	OPENSSL_cleanup();
}

#endif
