
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

NODISCARD static bool buffer_eq_impl(const void* const data1, const size_t size1,
                                     const void* const data2, const size_t size2) {
	if(size1 != size2) {
		return false;
	}

	return memcmp(data1, data2, size1) == 0;
}

NODISCARD bool readonly_buffer_eq(const ReadonlyBuffer buf1, const ReadonlyBuffer buf2) {
	return buffer_eq_impl(buf1.data, buf1.size, buf2.data, buf2.size);
}

NODISCARD bool sized_buffer_eq(const SizedBuffer buf1, const SizedBuffer buf2) {
	return buffer_eq_impl(buf1.data, buf1.size, buf2.data, buf2.size);
}

NODISCARD static SizedBuffer sized_buffer_dup_impl(const void* const data, const size_t size) {

	if(data == NULL) {
		return get_empty_sized_buffer();
	}

	if(size == 0) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	void* new_data = malloc(size);

	if(new_data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	memcpy(new_data, data, size);

	return (SizedBuffer){ .data = new_data, .size = size };
}

NODISCARD SizedBuffer sized_buffer_dup(const SizedBuffer buffer) {
	return sized_buffer_dup_impl(buffer.data, buffer.size);
}

NODISCARD tstr_view tstr_view_from_buffer(const SizedBuffer buffer) {
	return (tstr_view){ .data = buffer.data, .len = buffer.size };
}

NODISCARD tstr_view tstr_view_from_readonly_buffer(const ReadonlyBuffer buffer) {
	return (tstr_view){ .data = buffer.data, .len = buffer.size };
}

NODISCARD ReadonlyBuffer readonly_buffer_from_sized_buffer(const SizedBuffer buffer) {
	return (ReadonlyBuffer){ .data = (const void*)buffer.data, .size = buffer.size };
}

NODISCARD ReadonlyBuffer readonly_buffer_from_tstr(const tstr* const str) {
	return (ReadonlyBuffer){ .data = tstr_cstr(str), .size = tstr_len(str) };
}

NODISCARD SizedBuffer sized_buffer_from_tstr(tstr* const str) {
	return (SizedBuffer){ .data = tstr_data(str), .size = tstr_len(str) };
}

NODISCARD SizedBuffer sized_buffer_allocate_from_readonly_buffer(const ReadonlyBuffer buffer) {
	return sized_buffer_dup_impl(buffer.data, buffer.size);
}
