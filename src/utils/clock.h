
#pragma once

#include <stdint.h>
#include <time.h>

typedef struct {
	struct timespec value;
} Time;

// helpful macro for e.g sleeping

#define S_TO_MS_RATE 1000
#define S_TO_US_RATE (1000 * S_TO_MS_RATE)
#define S_TO_NS_RATE (1000 * S_TO_US_RATE)

#define S_TO_MS(x) ((x) * S_TO_MS_RATE)
#define S_TO_US(x) ((x) * S_TO_US_RATE)
#define S_TO_NS(x) ((x) * S_TO_NS_RATE)

bool get_monotonic_time(Time* time);

bool get_current_time(Time* time);

uint64_t get_time_in_seconds(Time time);

uint64_t get_time_in_milli_seconds(Time time);

uint64_t get_time_in_nano_seconds(Time time);

double get_time_in_seconds_exact(Time time);

double get_time_in_milli_seconds_exact(Time time);

double time_diff_in_exact_seconds(Time time1, Time time2);
