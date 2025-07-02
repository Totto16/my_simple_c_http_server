#pragma once

#include <stdint.h>

/*
 * return -1 if not found, returns the position (0 >= ) if found
 */
int strcasecontains(const char* value, const char* needle);
