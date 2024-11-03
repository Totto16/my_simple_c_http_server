
#pragma once

#include <stdlib.h>
#include <string.h>

// in here there are several utilities that are used across all .h and .c files
#include "utils.h"

// simple String builder used in http_protocol, its super convenient, self implemented

typedef struct {
	char* data;
	size_t currentSize;
} StringBuilder;

StringBuilder* string_builder_init(void);

// helper function that turns a normal string into a malloced one, so the lifetime is extended and
// he can be freed!

char* normalStringToMalloced(const char* notMallocedString);

// macro for appending, to used variable argument length conveniently, it uses the formatString
// (snprintf) and __string_builder_append method under the hood

#define string_builder_append(stringBuilder, format, ...) \
	{ \
		char* __append_buf = NULL; \
		formatString(&__append_buf, format, __VA_ARGS__); \
		__string_builder_append(stringBuilder, __append_buf); \
	}

// the actual append method, it accepts a string builder where to append and then appends the body
// string there
void __string_builder_append(StringBuilder* stringBuilder, char* string);

// simple wrapper if just a constant string has to be appended
void string_builder_append_single(StringBuilder* stringBuilder, const char* notMallocedString);

// attention the two methods to_string and get_string are different in that sense, that after
// to_string the Stringbuilder is freed and invalid, after get_string not!

// the struct or implementation can change, this function has to adapt, not thew user!
// ATTENTION: after this call the stringbuilder is destroyed! meaning the string you receive is a
// single malloced string you have to take care of
char* string_builder_to_string(StringBuilder* stringBuilder);

// the struct or implementation can change, this function has to adapt, not the user!
// after that call the stringbuilder is reusable and can be freed, appended uppon etc.
char* string_builder_get_string(StringBuilder* stringBuilder);

// just free the stringbuilder and the associated string
void string_builder_free(StringBuilder* stringBuilder);
