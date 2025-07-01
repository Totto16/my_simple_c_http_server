
#pragma once

#include "secure.h"

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
// it may be NULL!
NODISCARD char* read_string_from_connection(const ConnectionDescriptor* descriptor);

NODISCARD char* read_exact_bytes(const ConnectionDescriptor* descriptor, size_t n_bytes);
