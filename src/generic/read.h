
#pragma once

#include "secure.h"

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
char* readStringFromConnection(const ConnectionDescriptor* const descriptor);

char* readExactBytes(const ConnectionDescriptor* const descriptor, size_t n_bytes);

