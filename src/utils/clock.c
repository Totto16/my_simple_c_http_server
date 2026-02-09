

#include "./clock.h"
#include "utils/log.h"

#include <errno.h>
#include <locale.h>
#include <string.h>

// y2k -> 2038 bug avoidance
static_assert(sizeof(time_t) == sizeof(uint64_t));

// consistency check
static_assert(sizeof(typeof(((struct timespec*)0)->tv_nsec)) == sizeof(NanoTime));

NODISCARD Time time_from_struct(UnderlyingTimeValue value) {
	return (Time){ ._impl_value = value };
}

NODISCARD Time time_from_details(time_t seconds, NanoTime nanocseconds) {
	return (Time){ ._impl_value = {
		               .tv_sec = seconds,
		               .tv_nsec = nanocseconds,
		           } };
}

NODISCARD Time empty_time(void) {
	return time_from_details(0, 0);
}

NODISCARD Time time_from_seconds(time_t seconds) {
	return time_from_details(seconds, 0);
}

inline static bool valid_time(Time time) {

	/* Nanoseconds [0, 999'999'999] */
	if(time._impl_value.tv_nsec >= ((uint32_t)(S_TO_NS_RATE))) {
		return false;
	}

	if(time._impl_value.tv_nsec < 0) {
		return false;
	}

	return true;
}

static bool impl_get_time(Time* time, clockid_t clockid) {

	if(time == NULL) {
		return false;
	}

	int result = clock_gettime(clockid, &time->_impl_value);

	if(result != 0) {
		LOG_MESSAGE(LogLevelError, "Error in getting time for clock: %s\n", strerror(errno));
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

	return time._impl_value.tv_sec;
}

#define NS_TO_US(x) ((x) / S_TO_MS_RATE)
#define NS_TO_MS(x, TYPE) ((x) / ((TYPE)(S_TO_US_RATE)))
#define NS_TO_S(x, TYPE) ((x) / S_TO_NS_RATE)

uint64_t get_time_in_milli_seconds(Time time) {
	assert(valid_time(time) && "Valid time");

	uint64_t result = S_TO_MS(time._impl_value.tv_sec);

	result += NS_TO_MS(time._impl_value.tv_nsec, time_t);

	return result;
}

uint64_t get_time_in_nano_seconds(Time time) {
	assert(valid_time(time) && "Valid time");

	uint64_t result = S_TO_NS(time._impl_value.tv_sec, time_t);

	result += time._impl_value.tv_nsec;

	return result;
}

double get_time_in_seconds_exact(Time time) {
	assert(valid_time(time) && "Valid time");

	double result = (double)time._impl_value.tv_sec;

	result += ((double)time._impl_value.tv_nsec) / ((double)S_TO_NS_RATE);

	return result;
}

double get_time_in_milli_seconds_exact(Time time) {
	assert(valid_time(time) && "Valid time");

	double result = (double)time._impl_value.tv_sec;

	result += ((double)time._impl_value.tv_nsec) / ((double)S_TO_NS_RATE);

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

	TimeDiff result = { .diff = empty_time(), .neg = false };

	if(time2_ns > time1_ns) {
		result.neg = true;
		uint64_t temp = time1_ns;
		time1_ns = time2_ns;
		time2_ns = temp;
	}

	uint64_t diff_ns = time1_ns - time2_ns;

	time_t diff_s_part = (time_t)(diff_ns / ((uint64_t)(S_TO_NS_RATE)));

	result.diff._impl_value.tv_sec = diff_s_part;
	result.diff._impl_value.tv_nsec = (long)(diff_ns - (diff_s_part * ((time_t)(S_TO_NS_RATE))));

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

typedef struct {
	locale_t http_locale;
} GlobalClockData;

GlobalClockData g_clock_data = {
	.http_locale = (locale_t)0,
};

static void initialize_http_locale(void) {
	locale_t c_locale = newlocale(LC_TIME_MASK, "C", (locale_t)0);

	if(c_locale == (locale_t)0) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
		                   "Locale creation failed, this is likely an implementation error")
		exit(0);
	}

	g_clock_data.http_locale = c_locale;
}

void global_initialize_locale_for_http(void) {
	initialize_http_locale();
}

void global_free_locale_for_http(void) {

	if(g_clock_data.http_locale != (locale_t)0) {
		freelocale(g_clock_data.http_locale);
	}
}

static locale_t get_http_locale(void) {
	if(g_clock_data.http_locale == (locale_t)0) {
		initialize_http_locale();
	}

	return g_clock_data.http_locale;
}

#define FTP_TIME_FORMAT "%Y-%m-%d %H:%M"

// IMF-fixdate from https://datatracker.ietf.org/doc/html/rfc7231#section-7.1.1.1
#define HTTP1_1_RFC_7231_TIME_FORMAT "%a, %d %b %Y %H:%M:%S GMT"

#define COMMON_LOG_TIME_FORMAT "%d/%b/%Y:%H:%M:%S %z"

NODISCARD char* get_date_string(Time time, TimeFormat format) {

	const char* format_str = NULL;
	locale_t locale_to_use = (locale_t)0;
	bool use_utc = true;
	size_t max_bytes = 0;

	switch(format) {
		case TimeFormatFTP: {
			// see filezilla source code at src/engine/directorylistingparser.cpp:1094 at
			// CDirectoryListingParser::ParseUnixDateTime on why this exact format is used, it
			// has the most available information, while being recognized

			format_str = FTP_TIME_FORMAT;
			locale_to_use = (locale_t)0;
			use_utc = false;
			// just a guess, should suffice
			max_bytes = 0xFF;
			break;
		}
		case TimeFormatHTTP1Dot1: {
			// according to RFC c7231 - IMF-fixdate:
			// https://datatracker.ietf.org/doc/html/rfc7231#section-7.1.1.1
			format_str = HTTP1_1_RFC_7231_TIME_FORMAT;
			locale_to_use = get_http_locale();
			use_utc = true;
			// just a guess, should suffice
			max_bytes = 64;
			break;
		}
		case TimeFormatCommonLog: {
			// see: https://en.wikipedia.org/wiki/Common_Log_Format
			format_str = COMMON_LOG_TIME_FORMAT;
			locale_to_use = (locale_t)0;
			use_utc = false;
			// just a guess, should suffice
			max_bytes = 64;
			break;
		}
		default: {
			return NULL;
		}
	}

	char* date_str = (char*)malloc(max_bytes * sizeof(char));

	if(!date_str) {
		return NULL;
	}

	struct tm converted_time = { 0 };
	struct tm* convert_result = NULL;

	if(use_utc) {
		convert_result = gmtime_r(&time._impl_value.tv_sec, &converted_time);
	} else {
		convert_result = localtime_r(&time._impl_value.tv_sec, &converted_time);
	}

	if(!convert_result) {
		free(date_str);
		return NULL;
	}

	size_t result = 0;

	if(locale_to_use == (locale_t)0) {
		result = strftime(date_str, max_bytes, format_str, &converted_time);
	} else {
		result = strftime_l(date_str, max_bytes, format_str, &converted_time, locale_to_use);
	}

	if(result == 0) {
		free(date_str);
		return NULL;
	}

	date_str[result] = '\0';

	return date_str;
}
