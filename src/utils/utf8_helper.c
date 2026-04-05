

#include "./utf8_helper.h"

#include <utf8proc.h>

NODISCARD Utf8DataResult get_utf8_string(const void* data, long size) {

	utf8proc_int32_t* buffer = malloc(sizeof(utf8proc_int32_t) * size);

	if(!buffer) {
		return new_utf8_data_result_error(TSTR_STATIC_LIT("failed malloc"));
	}

	utf8proc_ssize_t result = utf8proc_decompose(
	    data, size, buffer, size,
	    (utf8proc_option_t)0); // NOLINT(cppcoreguidelines-narrowing-conversions,clang-analyzer-optin.core.EnumCastOutOfRange)

	if(result < 0) {
		free(buffer);
		return new_utf8_data_result_error(tstr_static_from_static_cstr(utf8proc_errmsg(result)));
	}

	if(result != size) {
		// truncate the buffer
		void* new_buffer = realloc(buffer, sizeof(utf8proc_int32_t) * result);

		if(!new_buffer) {
			free(buffer);
			return new_utf8_data_result_error(TSTR_STATIC_LIT("failed realloc"));
		}
		buffer = new_buffer;
	}

	Utf8Data utf8_data = {
		.data = buffer,
		.size = result,
	};

	return new_utf8_data_result_ok(utf8_data);
}

void free_utf8_data(Utf8Data data) {
	free(data.data);
}
