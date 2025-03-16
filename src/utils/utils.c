

#include "utils/utils.h"
#include "utils/log.h"

#include <stdint.h>

#include <errno.h>

// simple malloc Wrapper, using also memset to set everything to 0
void* mallocWithMemset(const size_t size, const bool initializeWithZeros) {
	void* result = malloc(size);
	if(result == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}
	if(initializeWithZeros) {
		// yes this could be done by calloc, but if you don't need that, its overhead!
		void* secondResult = memset(result, 0, size);
		if(result != secondResult) {
			// this shouldn't occur, but "better be safe than sorry"
			fprintf(stderr, "FATAL: Couldn't set the memory allocated to zeros!!\n");
			// free not really necessary, but also not that wrong
			free(result);
			return NULL;
		}
	}
	return result;
}

// simple realloc Wrapper, using also memset to set everything to 0
void* reallocWithMemset(void* previousPtr, const size_t oldSize, const size_t newSize,
                        const bool initializeWithZeros) {
	void* result = realloc(previousPtr, newSize);
	if(result == NULL) {
		fprintf(stderr, "ERROR: Couldn't reallocate memory!\n");
		return NULL;
	}
	if(initializeWithZeros && newSize > oldSize) {
		// yes this could be done by calloc, but if you don't need that, its overhead!
		void* secondResult = memset(((char*)result) + oldSize, 0, newSize - oldSize);
		if(((char*)result) + oldSize != secondResult) {
			// this shouldn't occur, but "better be safe than sorry"
			fprintf(stderr, "FATAL: Couldn't set the memory reallocated to zeros!!\n");
			// free not really necessary, but also not that wrong
			free(result);
			return NULL;
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
		LOG_MESSAGE(LogLevelWarn, "Couldn't parse the incorrect long: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	return result;
}

uint16_t parseU16Safely(const char* toParse, const char* description) {

	long result = parseLongSafely(toParse, description);

	if(result < 0) {
		fprintf(stderr,
		        "ERROR: Number not correct, '%ld' is negative, only positive numbers are allowed: "
		        "%s!\n",
		        result, description);
		exit(EXIT_FAILURE);
	}

	if(result > UINT16_MAX) {
		fprintf(stderr, "ERROR: Number not correct, '%ld' is too big for %s, the maximum is %d!\n",
		        result, description, UINT16_MAX);
		exit(EXIT_FAILURE);
	}

	return (uint16_t)result;
}

char* copy_cstr(char* input) {
	size_t length = strlen(input) + 1;

	char* result = malloc(length);

	if(!result) {
		return NULL;
	}

	memcpy(result, input, length);

	return result;
}
