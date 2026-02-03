

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

void free_parser(ParseState state) {
	free_sized_buffer(state.data);
}
