#include "string_builder.h"
#include "utils/log.h"

#include <stb/ds.h>

// this is just a dynamic array of chars, with the property, that the size is 1 more and at the end
// we have a 0 byte
struct StringBuilderImpl {
	STBDS_ARRAY(char) value;
};

StringBuilder* string_builder_init() {
	StringBuilder* result = (StringBuilder*)malloc(sizeof(StringBuilder));
	if(!result) {
		return NULL;
	}

	result->value = STBDS_ARRAY_EMPTY;

	return result;
}


// the actual append method, it accepts a string builder where to append and then appends the body
// string there
static int string_builder_append_string_impl(StringBuilder* stringBuilder, const char* string,
                                             size_t size) {

	if(size == 0) {
		return 0;
	}

	if(stringBuilder == NULL) {
		return -1;
	}

	size_t current_size = stbds_arrlenu(stringBuilder->value);

	// allocate 0 byte at the end, if needed
	size_t new_size = current_size + size + (current_size == 0 ? 1 : 0);

	stbds_arrsetlen(stringBuilder->value, new_size);

	stringBuilder->value[new_size - 1] = '\0';

	memcpy(stringBuilder->value + current_size - (current_size == 0 ? 0 : 1), string, size);

	return 0;
}

int string_builder_append_string(StringBuilder* stringBuilder, char* string) {
	size_t length = strlen(string);

	int result = string_builder_append_string_impl(stringBuilder, string, length);

	free(string);

	return result;
}

// simple wrapper if just a constant string has to be appended
int string_builder_append_single(StringBuilder* stringBuilder, const char* notMallocedString) {
	size_t length = strlen(notMallocedString);

	return string_builder_append_string_impl(stringBuilder, notMallocedString, length);
}

int string_builder_append_string_builder(StringBuilder* stringBuilder,
                                         StringBuilder** stringBuilder2) {

	if(stringBuilder2 == NULL) {
		return -1;
	}

	if(*stringBuilder2 == NULL) {
		return -2;
	}

	SizedBuffer stringBuilder2Buffer = string_builder_get_sized_buffer(*stringBuilder2);

	int result = string_builder_append_string_impl(stringBuilder, stringBuilder2Buffer.data,
	                                               stringBuilder2Buffer.size);

	free_string_builder(*stringBuilder2);
	*stringBuilder2 = NULL;

	return result;
}

NODISCARD char* string_builder_release_into_string(StringBuilder** stringBuilder) {

	if(stringBuilder == NULL) {
		return NULL;
	}

	if(*stringBuilder == NULL) {
		return NULL;
	}

	char* value = (*stringBuilder)->value;

	free(*stringBuilder);

	*stringBuilder = NULL;

	return value;
}

NODISCARD SizedBuffer string_builder_get_sized_buffer(StringBuilder* stringBuilder) {

	if(stringBuilder == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	size_t current_size = stbds_arrlenu(stringBuilder->value);

	size_t current_string_size = current_size == 0 ? 0 : current_size - 1;
	return (SizedBuffer){ .data = stringBuilder->value, .size = current_string_size };
}

// just free the stringbuilder and the associated string
void free_string_builder(StringBuilder* stringBuilder) {
	if(stringBuilder == NULL) {
		return;
	}

	stbds_arrfree(stringBuilder->value);
	free(stringBuilder);
}
