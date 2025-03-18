

#include "./clock.h"
#include "utils/log.h"
#include "utils/utils.h"

#include <errno.h>
#include <string.h>

// y2k -> 2038 bug avoidance
static_assert(sizeof(time_t) == sizeof(uint64_t));

inline static bool valid_time(Time time) {

	/* Nanoseconds [0, 999'999'999] */
	if(time.value.tv_nsec >= ((uint32_t)(S_TO_NS_RATE))) {
		return false;
	}

	if(time.value.tv_nsec < 0) {
		return false;
	}

	return true;
}

static bool impl_get_time(Time* time, clockid_t clockid) {

	if(time == NULL) {
		return false;
	}

	int result = clock_gettime(clockid, &time->value);

	if(result != 0) {
		LOG_MESSAGE(LogLevelError, "Error in getting time for clock: %s", strerror(errno));
		return false;
	}

	return true;
}

#ifdef __linux
#define CLOCK_MONOTONIC_OR_BETTER CLOCK_BOOTTIME
#else
#define CLOCK_MONOTONIC_OR_BETTER CLOCK_MONOTONIC
#endif

bool get_monotonic_time(Time* time) {
	return impl_get_time(time, CLOCK_MONOTONIC_OR_BETTER);
}

bool get_current_time(Time* time) {
	return impl_get_time(time, CLOCK_REALTIME);
}

uint64_t get_time_in_seconds(Time time) {
	assert(valid_time(time) && "Valid time");

	return time.value.tv_sec;
}

#define NS_TO_US(x) ((x) / S_TO_MS_RATE)
#define NS_TO_MS(x, TYPE) ((x) / ((TYPE)(S_TO_US_RATE)))
#define NS_TO_S(x, TYPE) ((x) / S_TO_NS_RATE)

uint64_t get_time_in_milli_seconds(Time time) {
	assert(valid_time(time) && "Valid time");

	uint64_t result = S_TO_MS(time.value.tv_sec);

	result += NS_TO_MS(time.value.tv_nsec, time_t);

	return result;
}

uint64_t get_time_in_nano_seconds(Time time) {
	assert(valid_time(time) && "Valid time");

	uint64_t result = S_TO_NS(time.value.tv_sec, time_t);

	result += time.value.tv_nsec;

	return result;
}

double get_time_in_seconds_exact(Time time) {
	assert(valid_time(time) && "Valid time");

	double result = (double)time.value.tv_sec;

	result += ((double)time.value.tv_nsec) / ((double)S_TO_NS_RATE);

	return result;
}

double get_time_in_milli_seconds_exact(Time time) {
	assert(valid_time(time) && "Valid time");

	double result = (double)time.value.tv_sec;

	result += ((double)time.value.tv_nsec) / ((double)S_TO_NS_RATE);

	return result;
}

typedef struct {
	Time diff;
	bool neg;
} TimeDiff;

// same as: time1 - time2;
static TimeDiff impl_time_diff(Time time1, Time time2) {

	assert(valid_time(time1) && "Valid time1");
	assert(valid_time(time2) && "Valid time2");

	uint64_t time1_ns = get_time_in_nano_seconds(time1);
	uint64_t time2_ns = get_time_in_nano_seconds(time2);

	TimeDiff result = { .diff = { .value = { .tv_sec = 0, .tv_nsec = 0 } }, .neg = false };

	if(time2_ns > time1_ns) {
		result.neg = true;
		uint64_t temp = time1_ns;
		time1_ns = time2_ns;
		time2_ns = temp;
	}

	uint64_t diff_ns = time1_ns - time2_ns;

	time_t diff_s_part = (time_t)(diff_ns / ((uint64_t)(S_TO_NS_RATE)));

	result.diff.value.tv_sec = diff_s_part;
	result.diff.value.tv_nsec = (long)(diff_ns - (diff_s_part * ((time_t)(S_TO_NS_RATE))));

	return result;
}

double time_diff_in_exact_seconds(Time time1, Time time2) {

	TimeDiff diff = impl_time_diff(time1, time2);

	double exact_seconds = get_time_in_seconds_exact(diff.diff);

	if(diff.neg) {
		exact_seconds *= -1.0;
	}

	return exact_seconds;
}
