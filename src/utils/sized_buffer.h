
#pragma once

#include <stdlib.h>
#include <tstr.h>

#include "./utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void* data;
	size_t size;
} SizedBuffer;

void free_sized_buffer(SizedBuffer buffer);

NODISCARD SizedBuffer get_empty_sized_buffer(void);

NODISCARD SizedBuffer allocate_sized_buffer(size_t size);

/**
 * @brief Get the exact clone object, note, that both pointers reference the same value, no deep
 * clone is performed
 *
 * @param buffer
 * @return SizedBuffer
 */
NODISCARD SizedBuffer sized_buffer_get_exact_clone(SizedBuffer buffer);

NODISCARD bool sized_buffer_eq(SizedBuffer buf1, SizedBuffer buf2);

NODISCARD SizedBuffer sized_buffer_dup(SizedBuffer buffer);

NODISCARD tstr_view tstr_view_from_buffer(SizedBuffer buffer);

typedef struct {
	const void* data;
	size_t size;
} ReadonlyBuffer;

NODISCARD tstr_view tstr_view_from_readonly_buffer(ReadonlyBuffer buffer);

NODISCARD ReadonlyBuffer readonly_buffer_from_sized_buffer(SizedBuffer buffer);

NODISCARD ReadonlyBuffer readonly_buffer_from_tstr(const tstr* str);

NODISCARD SizedBuffer sized_buffer_allocate_from_readonly_buffer(ReadonlyBuffer buffer);

NODISCARD bool readonly_buffer_eq(ReadonlyBuffer buf1, ReadonlyBuffer buf2);

#define SIZED_BUFFER_FMT "%.*s"

#define SIZED_BUFFER_FMT_ARGS(n) ((int)(n).size), ((char*)(n).data)

#ifdef __cplusplus
}
#endif
