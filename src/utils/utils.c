

#include "utils/utils.h"
#include "utils/log.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <sys/random.h>
#include <time.h>

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

NODISCARD GenericResult get_random_bytes(size_t size, OUT_PARAM(uint8_t) out_bytes) {

	if(size == 0) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	if(out_bytes == NULL) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	for(size_t i = 0; i < size;) {
		uint32_t random_byte = get_random_byte();
		uint8_t* random_byte_ptr = (uint8_t*)&random_byte;

		for(size_t j = 0; j < 4 && i < size; ++i, ++j) {
			out_bytes[i] = random_byte_ptr[j];
		}
	}

	return GENERIC_RES_OK();
}
