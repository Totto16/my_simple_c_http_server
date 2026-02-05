

#include "./utf8_helper.h"

#include <utf8proc.h>

NODISCARD Utf8DataResult get_utf8_string(const void* data, long size) {

	utf8proc_int32_t* buffer = malloc(sizeof(utf8proc_int32_t) * size);

	if(!buffer) {
		return (Utf8DataResult){ .has_error = true, .data = { .error = "failed malloc" } };
	}

	utf8proc_ssize_t result = utf8proc_decompose(
	    data, size, buffer, size,
	    (utf8proc_option_t)0); // NOLINT(cppcoreguidelines-narrowing-conversions,clang-analyzer-optin.core.EnumCastOutOfRange)

	if(result < 0) {
		free(buffer);
		return (Utf8DataResult){ .has_error = true, .data = { .error = utf8proc_errmsg(result) } };
	}

	if(result != size) {
		// truncate the buffer
		void* new_buffer = realloc(buffer, sizeof(utf8proc_int32_t) * result);

		if(!new_buffer) {
			free(buffer);
			return (Utf8DataResult){ .has_error = true, .data = { .error = "failed realloc" } };
		}
		buffer = new_buffer;
	}

	Utf8Data utf8_data = { .size = result, .data = buffer };

	return (Utf8DataResult){ .has_error = false, .data = { .result = utf8_data } };
}

void free_utf8_data(Utf8Data data) {
	free(data.data);
}
