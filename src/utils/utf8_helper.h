

#pragma once

#include "utils/utils.h"

typedef struct {
	int32_t* data;
	uint64_t size;
} Utf8Data;

typedef struct {
	bool has_error;
	union {
		Utf8Data result;
		const char* error;
	} data;
} Utf8DataResult;

NODISCARD Utf8DataResult get_utf8_string(const void* data, long size);
