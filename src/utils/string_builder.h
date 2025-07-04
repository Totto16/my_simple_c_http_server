
#pragma once

#include <stdlib.h>
#include <string.h>

// in here there are several utilities that are used across all .h and .c files
#include "utils/log.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"

// simple String builder used in http_protocol, its super convenient, self implemented

typedef struct StringBuilderImpl StringBuilder;

NODISCARD StringBuilder* string_builder_init(void);

// macro for appending, to used variable argument length conveniently, it uses the formatString
// (snprintf) and string_builder_append_string method under the hood

#define STRING_BUILDER_APPENDF(string_builder, statement, format, ...) \
	{ \
		if(string_builder != NULL) { \
			char* __append_buf = NULL; \
			FORMAT_STRING(&__append_buf, statement, format, __VA_ARGS__); \
			string_builder_append_string(string_builder, __append_buf); \
		} \
	}

// the actual append method, it accepts a string builder where to append and then appends the body
// string there
int string_builder_append_string(StringBuilder* string_builder, char* string);

int string_builder_append_string_builder(StringBuilder* string_builder,
                                         StringBuilder** string_builder2);

// simple wrapper if just a constant string has to be appended
int string_builder_append_single(StringBuilder* string_builder, const char* static_string);

NODISCARD char* string_builder_release_into_string(StringBuilder** string_builder);

NODISCARD size_t string_builder_get_string_size(StringBuilder* string_builder);

NODISCARD SizedBuffer string_builder_release_into_sized_buffer(StringBuilder** string_builder);

// free the stringbuilder
void free_string_builder(StringBuilder* string_builder);
