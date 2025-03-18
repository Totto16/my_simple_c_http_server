#ifndef SHA1_H
#define SHA1_H

/*
   SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   100% Public Domain
 */

#include "stdint.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
	uint32_t state[5]; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint32_t count[2]; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	unsigned char
	    buffer[64]; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
} SHA1_CTX;

void SHA1Transform(
    uint32_t state[5], // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    const unsigned char
        buffer[64]); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

void SHA1Init(SHA1_CTX* context);

void SHA1Update(SHA1_CTX* context, const unsigned char* data, uint32_t len);

void SHA1Final(
    unsigned char
        digest[20], // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    SHA1_CTX* context);

void SHA1(char* hash_out, const char* str, uint32_t len);

#if defined(__cplusplus)
}
#endif

#endif /* SHA1_H */
