#include "string_builder.h"
#include "utils/log.h"

#include <tvec.h>

TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(char)

typedef TVEC_TYPENAME(char) CString;

// this is just a dynamic array of chars, with the property, that the size is 1 more and at the end
// we have a 0 byte
struct StringBuilderImpl {
	CString value;
};

StringBuilder* string_builder_init() {
	StringBuilder* result = (StringBuilder*)malloc(sizeof(StringBuilder));
	if(!result) {
		return NULL;
	}

	result->value = TVEC_EMPTY(char);

	return result;
}

// the actual append method, it accepts a string builder where to append and then appends the body
// string there
static GenericResult string_builder_append_string_impl(StringBuilder* string_builder,
                                                       const char* string, size_t size) {

	if(size == 0) {
		return GENERIC_RES_OK();
	}

	if(string_builder == NULL) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	size_t current_size = TVEC_LENGTH(char, string_builder->value);

	// allocate 0 byte at the end, if needed
	size_t new_size = current_size + size + (current_size == 0 ? 1 : 0);

	const TvecResult allocate_res =
	    TVEC_ALLOCATE_UNINITIALIZED(char, &string_builder->value, new_size);
	OOM_ASSERT(allocate_res == TvecResultOk, "Vec allocate error");

	const TvecResult set_res = TVEC_SET_AT(char, &string_builder->value, new_size - 1, '\0');
	OOM_ASSERT(set_res == TvecResultOk, "Vec set at error");

	// TODO(Totto): make this a public function on the TVEC
	memcpy(string_builder->value.data + current_size - (current_size == 0 ? 0 : 1), string, size);

	return GENERIC_RES_OK();
}

GenericResult string_builder_append_string(StringBuilder* string_builder, char* string) {
	size_t length = strlen(string);

	const GenericResult result = string_builder_append_string_impl(string_builder, string, length);

	free(string);

	return result;
}

// simple wrapper if just a constant string has to be appended
GenericResult string_builder_append_single(StringBuilder* string_builder,
                                           const char* static_string) {
	size_t length = strlen(static_string);

	return string_builder_append_string_impl(string_builder, static_string, length);
}

GenericResult string_builder_append_tstr_static(StringBuilder* string_builder,
                                                tstr_static static_string) {
	return string_builder_append_string_impl(string_builder, static_string.ptr, static_string.len);
}

GenericResult string_builder_append_string_builder(StringBuilder* string_builder,
                                                   StringBuilder** string_builder2) {

	if(string_builder2 == NULL) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	if(*string_builder2 == NULL) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	SizedBuffer string_builder_2_buffer = string_builder_release_into_sized_buffer(string_builder2);

	const GenericResult result = string_builder_append_string_impl(
	    string_builder, string_builder_2_buffer.data, string_builder_2_buffer.size);

	free_sized_buffer(string_builder_2_buffer);

	return result;
}

NODISCARD char* string_builder_release_into_string(StringBuilder** string_builder) {

	if(string_builder == NULL) {
		return NULL;
	}

	if(*string_builder == NULL) {
		return NULL;
	}

	// getting the data and then just free the container around, this is safe to do with the TVEC
	char* value = (*string_builder)->value.data;

	free(*string_builder);

	*string_builder = NULL;

	return value;
}

NODISCARD size_t string_builder_get_string_size(StringBuilder* string_builder) {

	if(string_builder == NULL) {
		return 0;
	}

	size_t current_size = TVEC_LENGTH(char, string_builder->value);

	size_t current_string_size = current_size == 0 ? 0 : current_size - 1;
	return current_string_size;
}

NODISCARD SizedBuffer string_builder_release_into_sized_buffer(StringBuilder** string_builder) {

	if(string_builder == NULL) {
		return get_empty_sized_buffer();
	}

	if(*string_builder == NULL) {
		return get_empty_sized_buffer();
	}

	if((*string_builder)->value.data == NULL) {
		free_string_builder(*string_builder);
		return get_empty_sized_buffer();
	}

	size_t current_size = TVEC_LENGTH(char, (*string_builder)->value);

	size_t current_string_size = current_size == 0 ? 0 : current_size - 1;

	// getting the data and then just free the container around, this is safe to do with the TVEC
	char* value = (*string_builder)->value.data;

	SizedBuffer result = { .data = value, .size = current_string_size };

	free(*string_builder);

	*string_builder = NULL;

	return result;
}

// just free the stringbuilder and the associated string
void free_string_builder(StringBuilder* string_builder) {
	if(string_builder == NULL) {
		return;
	}

	TVEC_FREE(char, &string_builder->value);
	free(string_builder);
}
