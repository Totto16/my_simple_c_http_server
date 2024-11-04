#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define checkResultForThreadErrorAndReturn(errorString) \
	checkForThreadError(result, errorString, return EXIT_FAILURE;)

#define checkResultForThreadErrorAndExit(errorString) \
	checkForThreadError(result, errorString, exit(EXIT_FAILURE);)

#define checkResultForErrorAndExit(errorString) \
	checkForError(result, errorString, exit(EXIT_FAILURE););

// simple malloc Wrapper, using also memset to set everything to 0
void* mallocOrFail(const size_t size, const bool initializeWithZeros);
