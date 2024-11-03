
#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: use c23 builtin, if available
// see e.g. https://www.gnu.org/software/gnulib/manual/html_node/Attributes.html
#define NODISCARD __attribute__((__warn_unused_result__))

#define UNUSED(v) ((void)v)

// cool trick from here:
// https://stackoverflow.com/questions/777261/avoiding-unused-variables-warnings-when-using-assert-in-a-release-build
#ifdef NDEBUG
#define assert(x) \
	do { \
		UNUSED(x); \
	} while(false)
#else

#include <assert.h>

#endif

// uses snprintf feature with passing NULL,0 as first two arguments to automatically determine the
// required buffer size, for more read man page
// for variadic functions its easier to use macro
// magic, attention, use this function in the right way, you have to prepare a char* that is set to
// null, then it works best! snprintf is safer then sprintf, since it guarantees some things, also
// it has a failure indicator
#define formatString(toStore, format, ...) \
	{ \
		char* internalBuffer = *toStore; \
		if(internalBuffer != NULL) { \
			free(internalBuffer); \
		} \
		int toWrite = snprintf(NULL, 0, format, __VA_ARGS__) + 1; \
		internalBuffer = (char*)mallocOrFail(toWrite * sizeof(char), true); \
		int written = snprintf(internalBuffer, toWrite, format, __VA_ARGS__); \
		if(written >= toWrite) { \
			fprintf( \
			    stderr, \
			    "ERROR: Snprint did write more bytes then it had space in the buffer, available " \
			    "space:'%d', actually written:'%d'!\n", \
			    (toWrite) - 1, written); \
			free(internalBuffer); \
			exit(EXIT_FAILURE); \
		} \
		*toStore = internalBuffer; \
	} \
	if(*toStore == NULL) { \
		fprintf(stderr, "ERROR: snprintf Macro gone wrong: '%s' is pointing to NULL!\n", \
		        #toStore); \
		exit(EXIT_FAILURE); \
	}

// simple error helper macro, with some more used "overloads"
#define checkForError(toCheck, errorString, statement) \
	do { \
		if(toCheck == -1) { \
			perror(errorString); \
			statement; \
		} \
	} while(false)

#define checkForThreadError(toCheck, errorString, statement) \
	do { \
		if(toCheck != 0) { \
			/*pthread function don't set errno, but return the error value \
			 * directly*/ \
			fprintf(stderr, "%s: %s\n", errorString, strerror(toCheck)); \
			statement; \
		} \
	} while(false)

#define checkResultForThreadError(errorString, statement) \
	checkForThreadError(result, errorString, statement)

#define checkResultForThreadErrorAndReturn(errorString) \
	checkForThreadError(result, errorString, return EXIT_FAILURE;)

#define checkResultForThreadErrorAndExit(errorString) \
	checkForThreadError(result, errorString, exit(EXIT_FAILURE);)

#define checkResultForErrorAndExit(errorString) \
	checkForError(result, errorString, exit(EXIT_FAILURE););

// simple malloc Wrapper, using also memset to set everything to 0
void* mallocOrFail(const size_t size, const bool initializeWithZeros);

// simple realloc Wrapper, using also memset to set everything to 0
void* reallocOrFail(void* previousPtr, const size_t oldSize, const size_t newSize,
                    const bool initializeWithZeros);

// copied from exercises before (PS 1-7, selfmade), it safely parses a long!
long parseLongSafely(const char* toParse, const char* description);

uint16_t parseU16Safely(const char* toParse, const char* description);

// a hacky but good and understandable way that is used with pthread functions
// to annotate which type the really represent
#define any void*

#define anyType(type) /* Type helper for readability */ any
