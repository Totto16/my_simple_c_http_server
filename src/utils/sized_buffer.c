
#include "./sized_buffer.h"

void free_sized_buffer(SizedBuffer buffer) {
	free(buffer.data);
}

NODISCARD SizedBuffer get_empty_sized_buffer(void) {
	return (SizedBuffer){ .data = NULL, .size = 0 };
}

NODISCARD SizedBuffer sized_buffer_get_exact_clone(SizedBuffer buffer) {
	return (SizedBuffer){ .data = buffer.data, .size = buffer.size };
}

NODISCARD int sized_buffer_cmp(SizedBuffer buf1, SizedBuffer buf2) {
	if(buf1.size != buf2.size) {
		return (int)buf2.size - (int)buf1.size;
	}

	return memcmp(buf1.data, buf2.data, buf1.size);
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
