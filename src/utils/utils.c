

#include "utils/utils.h"
#include "utils/log.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <sys/random.h>
#include <time.h>

// copied from exercises before (PS 1-7, selfmade), it safely parses a long!
static long parse_long_impl(const char* to_parse, const char* description,
                            OUT_PARAM(bool) success) {
	// this is just allocated, so that strtol can write an address into it,
	// therefore it doesn't need to be initialized
	char* endpointer = NULL;
	// reseting errno, since it's not guaranteed to be that, but strtol can return some values that
	// generally are also valid, so errno is the only REAL and consistent method of checking for
	// error
	errno = 0;
	// using strtol, string to long, since atoi doesn't report errors that well
	long result =
	    strtol(to_parse, &endpointer,
	           10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	// it isn't a number, if either errno is set or if the endpointer is not a '\0
	if(*endpointer != '\0') {
		if(description != NULL) {

			LOG_MESSAGE(LogLevelError,
			            "Couldn't parse the incorrect long %s for the argument %s!\n", to_parse,
			            description);
		}
		*success = false;
		return 0;

	} else if(errno != 0) {

		if(description != NULL) {
			LOG_MESSAGE(LogLevelWarn, "Couldn't parse the incorrect long: %s\n", strerror(errno));
		}
		*success = false;
		return 0;
	}

	*success = true;
	return result;
}

// copied from exercises before (PS 1-7, selfmade), it safely parses a long!
long parse_long_safely(const char* to_parse, const char* description) {

	bool success = true;

	long result = parse_long_impl(to_parse, description, &success);

	if(!success) {
		exit(EXIT_FAILURE);
	}

	return result;
}

NODISCARD long parse_long(const char* to_parse, OUT_PARAM(bool) success) {

	return parse_long_impl(to_parse, NULL, success);
}

static_assert(sizeof(unsigned long long) == sizeof(size_t));

static size_t parse_size_t_impl(const char* to_parse, OUT_PARAM(bool) success) {
	// this is just allocated, so that strtol can write an address into it,
	// therefore it doesn't need to be initialized
	char* endpointer = NULL;
	// reseting errno, since it's not guaranteed to be that, but strtol can return some values that
	// generally are also valid, so errno is the only REAL and consistent method of checking for
	// error
	errno = 0;
	// using strtol, string to long, since atoi doesn't report errors that well
	size_t result =
	    strtoull(to_parse, &endpointer,
	             10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	// it isn't a number, if either errno is set or if the endpointer is not a '\0
	if(*endpointer != '\0') {
		*success = false;
		return 0;

	} else if(errno != 0) {
		*success = false;
		return 0;
	}

	*success = true;
	return result;
}

NODISCARD size_t parse_size_t(const char* to_parse, OUT_PARAM(bool) success) {
	return parse_size_t_impl(to_parse, success);
}

NODISCARD uint16_t parse_u16_safely(const char* to_parse, const char* description) {

	long result = parse_long_safely(to_parse, description);

	if(result < 0) {
		LOG_MESSAGE(LogLevelError,
		            "Number not correct, '%ld' is negative, only positive numbers are allowed: "
		            "%s!\n",
		            result, description);
		exit(EXIT_FAILURE);
	}

	if(result > UINT16_MAX) {
		LOG_MESSAGE(LogLevelError,
		            "Number not correct, '%ld' is too big for %s, the maximum is %d!\n", result,
		            description, UINT16_MAX);
		exit(EXIT_FAILURE);
	}

	return (uint16_t)result;
}

NODISCARD float parse_float(const char* const value) {
	char* endpointer = NULL;
	errno = 0;
	float result = strtof(value, &endpointer);

	// it isn't a number, if either errno is set or if the endpointer is not a '\0
	if(*endpointer != '\0' || errno != 0) {
		return NAN;
	}

	return result;
}

NODISCARD uint32_t get_random_byte(void) {
#ifdef __APPLE__
	srandom(time(NULL));
	uint32_t random_bytes = random();
#else
	uint32_t random_bytes = 0;
	ssize_t result = getrandom((uint8_t*)(&random_bytes), sizeof(uint32_t), 0);
	if(result != sizeof(uint32_t)) {
		if(result < 0) {
			LOG_MESSAGE(LogLevelWarn, "Get random failed: %s\n", strerror(errno));
		}

		unsigned int seed = time(NULL);

		// use rand_r like normal rand:
		random_bytes = rand_r(&seed);
	}
#endif
	return random_bytes;
}

NODISCARD uint32_t get_random_byte_in_range(uint32_t min, uint32_t max) {

	assert(min < max);

	uint32_t value = get_random_byte();

	return (value % (max - min)) + min;
}

NODISCARD int get_random_bytes(size_t size, OUT_PARAM(uint8_t) out_bytes) {

	if(size == 0) {
		return -1;
	}

	if(out_bytes == NULL) {
		return -2;
	}

	for(size_t i = 0; i < size;) {
		uint32_t random_byte = get_random_byte();
		uint8_t* random_byte_ptr = (uint8_t*)&random_byte;

		for(size_t j = 0; j < 4 && i < size; ++i, ++j) {
			out_bytes[i] = random_byte_ptr[j];
		}
	}

	return 0;
}
