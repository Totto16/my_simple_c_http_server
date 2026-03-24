
#include "./sized_buffer.h"

void free_sized_buffer(SizedBuffer buffer) {
	free(buffer.data);
}

NODISCARD SizedBuffer get_empty_sized_buffer(void) {
	return (SizedBuffer){ .data = NULL, .size = 0 };
}

NODISCARD SizedBuffer allocate_sized_buffer(size_t size) {

	void* data = malloc(size);

	if(data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	return (SizedBuffer){ .data = data, .size = size };
}

NODISCARD SizedBuffer sized_buffer_get_exact_clone(SizedBuffer buffer) {
	return (SizedBuffer){ .data = buffer.data, .size = buffer.size };
}

NODISCARD bool sized_buffer_eq(const SizedBuffer buf1, const SizedBuffer buf2) {
	return sized_buffer_eq_data(buf1, buf2.data, buf2.size);
}

NODISCARD bool sized_buffer_eq_data(const SizedBuffer buf1, const void* const data,
                                    const size_t size) {
	if(buf1.size != size) {
		return false;
	}

	return memcmp(buf1.data, data, buf1.size) == 0;
}

NODISCARD SizedBuffer sized_buffer_dup(SizedBuffer buffer) {

	if(buffer.data == NULL) {
		return buffer;
	}

	if(buffer.size == 0) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	void* new_data = malloc(buffer.size);

	if(new_data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	memcpy(new_data, buffer.data, buffer.size);

	return (SizedBuffer){ .data = new_data, .size = buffer.size };
}

NODISCARD tstr_view tstr_view_from_buffer(const SizedBuffer buffer) {
	return (tstr_view){ .data = buffer.data, .len = buffer.size };
}

NODISCARD ReadonlyBuffer readonly_buffer_from_sized_buffer(const SizedBuffer buffer) {
	return (ReadonlyBuffer){ .data = (const void*)buffer.data, .size = buffer.size };
}

NODISCARD ReadonlyBuffer readonly_buffer_from_tstr(const tstr* const str) {
	return (ReadonlyBuffer){ .data = tstr_cstr(str), .size = tstr_len(str) };
}
