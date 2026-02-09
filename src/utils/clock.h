
#pragma once

#include <stdint.h>
#include <time.h>

#include "./utils.h"

typedef struct timespec UnderlyingTimeValue;

typedef time_t NanoTime;

typedef struct {
	UnderlyingTimeValue _impl_value; // NOLINT(readability-identifier-naming)
} Time;

// helpful macro for e.g sleeping

#define S_TO_MS_RATE 1000
#define S_TO_US_RATE (1000 * S_TO_MS_RATE)
#define S_TO_NS_RATE (1000 * S_TO_US_RATE)

#define S_TO_MS(x) ((x) * S_TO_MS_RATE)
#define S_TO_US(x) ((x) * S_TO_US_RATE)
#define S_TO_NS(x, TYPE) ((x) * ((TYPE)(S_TO_NS_RATE)))

NODISCARD Time time_from_struct(UnderlyingTimeValue value);

NODISCARD Time time_from_details(time_t seconds, NanoTime nanocseconds);

NODISCARD Time empty_time(void);

NODISCARD Time time_from_seconds(time_t seconds);

NODISCARD bool get_monotonic_time(OUT_PARAM(Time) time);

NODISCARD bool get_current_time(OUT_PARAM(Time) time);

NODISCARD uint64_t get_time_in_seconds(Time time);

NODISCARD uint64_t get_time_in_milli_seconds(Time time);

NODISCARD uint64_t get_time_in_nano_seconds(Time time);

NODISCARD double get_time_in_seconds_exact(Time time);

NODISCARD double get_time_in_milli_seconds_exact(Time time);

NODISCARD double time_diff_in_exact_seconds(Time time1, Time time2);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	TimeFormatFTP = 0,
	TimeFormatHTTP1Dot1,
	TimeFormatCommonLog,
} TimeFormat;

/**
 * @brief Get the date string of time value, not that accurate, only up to seconds
 *
 * @param time
 * @param format
 * @return NODISCARD*
 */
NODISCARD char* get_date_string(Time time, TimeFormat format);

void global_initialize_locale_for_http(void);

void global_free_locale_for_http(void);
