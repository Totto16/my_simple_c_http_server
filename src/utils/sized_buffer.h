
#pragma once

#include <stdlib.h>

#include "./utils.h"

typedef struct {
	void* data;
	size_t size;
} SizedBuffer;

void free_sized_buffer(SizedBuffer buffer);

NODISCARD SizedBuffer get_empty_sized_buffer(void);

/**
 * @brief Get the exact clone object, note, that both pointers reference the same value, no deep
 * clone is performed
 *
 * @param buffer
 * @return SizedBuffer
 */
NODISCARD SizedBuffer sized_buffer_get_exact_clone(SizedBuffer buffer);
