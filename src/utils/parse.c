

#include "./parse.h"

NODISCARD static Byte* parser_get_until_delimiter_fixed_impl(ParseState* state,
                                                             const SizedBuffer delimiter) {

	size_t delimiter_index = 0;
	Byte* delimiter_bytes = (Byte*)delimiter.data;

	Byte* data = (Byte*)state->data.data;

	for(size_t i = state->cursor; i < state->data.size; ++i) {

		const Byte data_byte = data[i];

		const Byte delimiter_byte = delimiter_bytes[delimiter_index];

		if(data_byte == delimiter_byte) {
			delimiter_index++;

			if(delimiter_index >= delimiter.size) {

				state->cursor = i + 1;
				return data + (i + 1 - delimiter.size);
			}
		} else {
			delimiter_index = 0;
		}
	}

	return NULL;
}

NODISCARD Byte* parser_get_until_delimiter(ParseState* state, const char* const delimiter) {
	SizedBuffer fixed = { .data = (void*)delimiter, .size = strlen(delimiter) };
	return parser_get_until_delimiter_fixed_impl(state, fixed);
}

NODISCARD Byte* parser_get_until_delimiter_fixed(ParseState* state, const SizedBuffer delimiter) {
	return parser_get_until_delimiter_fixed_impl(state, delimiter);
}

NODISCARD SizedBuffer parser_get_until_end(ParseState* const state) {

	size_t size = state->data.size - state->cursor;

	SizedBuffer result = { .data = state->data.data, .size = size };

	state->cursor += size;

	return result;
}

NODISCARD SizedBuffer parser_get_until(ParseState* const state, const size_t amount) {

	size_t remaining_size = state->data.size - state->cursor;

	if(remaining_size < amount) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	SizedBuffer result = { .data = state->data.data, .size = amount };

	state->cursor += amount;

	return result;
}

void free_parser(ParseState state) {
	free_sized_buffer(state.data);
}
