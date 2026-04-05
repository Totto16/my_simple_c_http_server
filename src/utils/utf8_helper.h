

#pragma once

#include "utils/utils.h"

typedef int32_t Utf8Codepoint;

typedef struct {
	Utf8Codepoint* data;
	uint64_t size;
} Utf8Data;

GENERATE_VARIANT_ALL_UTF8_DATA_RESULT()

NODISCARD Utf8DataResult get_utf8_string(const void* data, long size);

void free_utf8_data(Utf8Data data);
