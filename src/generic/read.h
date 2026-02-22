
#pragma once

#include "secure.h"
#include "utils/sized_buffer.h"
#include <tstr.h>

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
// it may be NULL!
// TODO: remove
NODISCARD char* NULLABLE
read_string_from_connection_deprecated(const ConnectionDescriptor* descriptor);

typedef struct {
	bool is_error;
	union {
		tstr str;
	} data;
} ReadTStrResult;

/**
 * @brief Reads bytes into a buffer from a connection
 * @note due to legacy reasons, the buffer has a 0 byte after the end!
 *
 * @param descriptor
 * @return {SizedBuffer}
 */
// TODO: use this function, when needing a SizedBuffer, so return SizedBuffer
NODISCARD ReadTStrResult read_buffer_from_connection(const ConnectionDescriptor* descriptor);

NODISCARD ReadTStrResult read_tstr_from_connection(const ConnectionDescriptor* descriptor);
