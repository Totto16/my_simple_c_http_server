

#include "./hash.h"
#include "utils/log.h"

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

#ifdef _SIMPLE_SERVER_USE_OPENSSL_FOR_HASHING

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
#include <openssl/evp.h>

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
