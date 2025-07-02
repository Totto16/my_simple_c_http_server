
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
