#include "utils/string_helper.h"

#include <string.h>

int strcasecontains(const char* value, const char* needle) {
	int value_len = (int)strlen(value);
	int needle_len = (int)strlen(needle);

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
