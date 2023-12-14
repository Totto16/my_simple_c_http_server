
// header guard
#ifndef _CUSTOM_UTILS_H_
#define _CUSTOM_UTILS_H_

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cool trick from here:
// https://stackoverflow.com/questions/777261/avoiding-unused-variables-warnings-when-using-assert-in-a-release-build
#ifdef NDEBUG
#define assert(x) \
	do { \
		(void)sizeof(x); \
	} while(0)
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
			    (toWrite)-1, written); \
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
void* mallocOrFail(const size_t size, const bool initializeWithZeros) {
	void* result = malloc(size);
	if(result == NULL) {
		fprintf(stderr, "ERROR: Couldn't allocate memory!\n");
		exit(EXIT_FAILURE);
	}
	if(initializeWithZeros) {
		// yes this could be done by calloc, but if you don't need that, its overhead!
		void* secondResult = memset(result, 0, size);
		if(result != secondResult) {
			// this shouldn't occur, but "better be safe than sorry"
			fprintf(stderr, "FATAL: Couldn't set the memory allocated to zeros!!\n");
			// free not really necessary, but also not that wrong
			free(result);
			exit(EXIT_FAILURE);
		}
	}
	return result;
}

// simple realloc Wrapper, using also memset to set everything to 0
void* reallocOrFail(void* previousPtr, const size_t oldSize, const size_t newSize,
                    const bool initializeWithZeros) {
	void* result = realloc(previousPtr, newSize);
	if(result == NULL) {
		fprintf(stderr, "ERROR: Couldn't reallocate memory!\n");
		exit(EXIT_FAILURE);
	}
	if(initializeWithZeros && newSize > oldSize) {
		// yes this could be done by calloc, but if you don't need that, its overhead!
		void* secondResult = memset(((char*)result) + oldSize, 0, newSize - oldSize);
		if(((char*)result) + oldSize != secondResult) {
			// this shouldn't occur, but "better be safe than sorry"
			fprintf(stderr, "FATAL: Couldn't set the memory reallocated to zeros!!\n");
			// free not really necessary, but also not that wrong
			free(result);
			exit(EXIT_FAILURE);
		}
	}
	return result;
}

// copied from exercises before (PS 1-7, selfmade), it safely parses a long!
long parseLongSafely(const char* toParse, const char* description) {
	// this is just allocated, so that strtol can write an address into it,
	// therefore it doesn't need to be initialized
	char* endpointer;
	// reseting errno, since it's not guaranteed to be that, but strtol can return some values that
	// generally are also valid, so errno is the only REAL and consistent method of checking for
	// error
	errno = 0;
	// using strtol, string to long, since atoi doesn't report errors that well
	long result = strtol(toParse, &endpointer, 10);

	// it isn't a number, if either errno is set or if the endpointer is not a '\0
	if(*endpointer != '\0') {
		fprintf(stderr, "ERROR: Couldn't parse the incorrect long %s for the argument %s!\n",
		        toParse, description);
		exit(EXIT_FAILURE);
	} else if(errno != 0) {
		perror("Couldn't parse the incorrect long");
		exit(EXIT_FAILURE);
	}

	return result;
}

// a hacky but good and understandable way that is used with pthread functions
// to annotate which type the really represent
#define any void*

#define anyType(type) /* Type helper for readability */ any

#endif
