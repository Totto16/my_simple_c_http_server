#include "string_helper.h"
#include "utils.h"

#include <sha1.h>
#include <string.h>

int strcasecontains(const char* value, const char* needle) {
	int value_len = strlen(value);
	int needle_len = strlen(needle);

	if(value_len < needle_len) {
		return -1;
	}

	int difference = value_len - needle_len;

	for(int i = 0; i <= difference; ++i) {
		int result = strncasecmp(value + i, needle, needle_len);
		if(result == 0) {
			return i;
		}
	}

	return -1;
}

uint8_t* sha1(const char* string) {

	SHA1_CTX sha;

	SHA1Init(&sha);

	SHA1Update(&sha, (uint8_t*)string, strlen(string));

	uint8_t* result = mallocOrFail(SHA1_LEN * sizeof(uint8_t), false);

	SHA1Final(result, &sha);

	return result;
}
