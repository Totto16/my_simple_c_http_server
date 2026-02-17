
#pragma once

#include "secure.h"
#include "utils/sized_buffer.h"

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
// it may be NULL!
NODISCARD char* NULLABLE read_string_from_connection(const ConnectionDescriptor* descriptor);

/**
 * @brief Reads bytes into a buffer from a connection
 * @note due to legacy reasons, the buffer has a 0 byte after the end!
 *
 * @param descriptor
 * @return {SizedBuffer}
 */
NODISCARD SizedBuffer read_buffer_from_connection(const ConnectionDescriptor* descriptor);

