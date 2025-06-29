
#pragma once

#include <stdlib.h>
#include <string.h>

// in here there are several utilities that are used across all .h and .c files
#include "utils/log.h"
#include "utils/utils.h"

// simple String builder used in http_protocol, its super convenient, self implemented

typedef struct {
	char* data;
	size_t currentSize;
} StringBuilder;

NODISCARD StringBuilder* string_builder_init(void);

// helper function that turns a normal string into a malloced one, so the lifetime is extended and
// he can be freed!

char* normalStringToMalloced(const char* notMallocedString);

// macro for appending, to used variable argument length conveniently, it uses the formatString
// (snprintf) and string_builder_append_string method under the hood

#define string_builder_append(stringBuilder, statement, format, ...) \
	{ \
		char* __append_buf = NULL; \
		formatString(&__append_buf, statement, format, __VA_ARGS__); \
		string_builder_append_string(stringBuilder, __append_buf); \
	}

// TODO(Totto): make them NODISCARD

// the actual append method, it accepts a string builder where to append and then appends the body
// string there
int string_builder_append_string(StringBuilder* stringBuilder, char* string);

int string_builder_append_string_builder(StringBuilder* stringBuilder,
                                         StringBuilder* stringBuilder2);

// simple wrapper if just a constant string has to be appended
int string_builder_append_single(StringBuilder* stringBuilder, const char* notMallocedString);

// attention the two methods to_string and get_string are different in that sense, that after
// to_string the Stringbuilder is freed and invalid, after get_string not!

/**
 * @brief  the struct or implementation can change, this function has to adapt, not the user!
 * ATTENTION: after this call the stringbuilder is destroyed! meaning the string you receive is a
 * single malloced string you have to take care of
 *
 * @deprecated use dedicated functions, and don't treat string builder as string, as strlen for the
 * length is not effective!
 * @param stringBuilder
 * @return char*
 */
NODISCARD char* string_builder_to_string_deprecated(StringBuilder* stringBuilder);

/**
 * @brief  the struct or implementation can change, this function has to adapt, not the user! after
 * that call the stringbuilder is reusable and can be freed, appended uppon etc.
 *
 * @deprecated use dedicated functions, and don't treat string builder as string, as strlen for the
 * length is not effective!
 * @param stringBuilder
 * @return char*
 */
NODISCARD char* string_builder_get_string_deprecated(StringBuilder* stringBuilder);

NODISCARD SizedBuffer string_builder_get_sized_buffer(StringBuilder* stringBuilder);

// just free the stringbuilder and the associated string
void free_string_builder(StringBuilder* stringBuilder);
