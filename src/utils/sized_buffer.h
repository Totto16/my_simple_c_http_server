
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "./utils.h"

typedef struct {
	void* data;
	size_t size;
} SizedBuffer;

void free_sized_buffer(SizedBuffer buffer);

NODISCARD SizedBuffer get_empty_sized_buffer(void);

NODISCARD SizedBuffer allocate_sized_buffer(size_t size);

NODISCARD SizedBuffer sized_buffer_from_cstr(const char* value);

/**
 * @brief Get the exact clone object, note, that both pointers reference the same value, no deep
 * clone is performed
 *
 * @param buffer
 * @return SizedBuffer
 */
NODISCARD SizedBuffer sized_buffer_get_exact_clone(SizedBuffer buffer);

NODISCARD int sized_buffer_cmp(SizedBuffer buf1, SizedBuffer buf2);

NODISCARD int sized_buffer_cmp_with_data(SizedBuffer buf1, const void* data, size_t size);

NODISCARD SizedBuffer sized_buffer_dup(SizedBuffer buffer);

#ifdef __cplusplus
}
#endif
