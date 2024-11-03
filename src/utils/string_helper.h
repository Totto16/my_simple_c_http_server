#pragma once

#include <stdint.h>

/*
 * return -1 if not found, returns the position (0 >= ) if found
 */
int strcasecontains(const char* value, const char* needle);

#define SHA1_LEN 20

uint8_t* sha1(const char* string);
