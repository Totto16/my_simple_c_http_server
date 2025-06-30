
#pragma once

#include <stdlib.h>
#include <string.h>

// in here there are several utilities that are used across all .h and .c files
#include "utils/log.h"
#include "utils/utils.h"

// simple String builder used in http_protocol, its super convenient, self implemented

typedef struct StringBuilderImpl StringBuilder;

NODISCARD StringBuilder* string_builder_init(void);

// helper function that turns a normal string into a malloced one, so the lifetime is extended and
// he can be freed!

NODISCARD char* normalStringToMalloced(const char* notMallocedString);

// macro for appending, to used variable argument length conveniently, it uses the formatString
// (snprintf) and string_builder_append_string method under the hood

#define string_builder_append(stringBuilder, statement, format, ...) \
	{ \
		if(stringBuilder != NULL) { \
			char* __append_buf = NULL; \
			formatString(&__append_buf, statement, format, __VA_ARGS__); \
			string_builder_append_string(stringBuilder, __append_buf); \
		} \
	}

// the actual append method, it accepts a string builder where to append and then appends the body
// string there
int string_builder_append_string(StringBuilder* stringBuilder, char* string);

int string_builder_append_string_builder(StringBuilder* stringBuilder,
                                         StringBuilder** stringBuilder2);

// simple wrapper if just a constant string has to be appended
int string_builder_append_single(StringBuilder* stringBuilder, const char* notMallocedString);

NODISCARD char* string_builder_release_into_string(StringBuilder** stringBuilder);

NODISCARD SizedBuffer string_builder_get_sized_buffer(StringBuilder* stringBuilder);

// free the stringbuilder
void free_string_builder(StringBuilder* stringBuilder);
