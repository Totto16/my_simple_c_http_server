

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>

#include "log.h"
#include "utils/utils.h"

#include <pthread.h>

#define DEFAULT_LOG_LEVEL LogLevelInfo

// global state

typedef struct {
	LogLevel log_level;
	pthread_mutex_t mutex;
} GlobalLogState;

static GlobalLogState __global_log_entry = { .log_level = DEFAULT_LOG_LEVEL };

// thread state

typedef struct {
	const char* name;
} ThreadState;

static _Thread_local ThreadState __log_thread_state = { .name = NULL };

bool __should_log(LogLevel level) {
	if(__global_log_entry.log_level == LogLevelOff) {
		return false;
	}

	return __global_log_entry.log_level <= level;
}

bool __should_log_to_stderr(LogLevel level) {
	return level >= LogLevelError;
}

bool __log_should_use_color(void) {
	return isatty(STDIN_FILENO);
}

#define NC "\033[0m" // NO COLOR

#define BOLD "\033[1m"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define LBLUE "\033[36m"
#define LGRAY "\033[37m"
#define DEFAULT "\033[39m"
#define WHITE "\033[97m"

const char* __get_level_name(LogLevel level, bool color) {

	if(!color) {

		switch(level) {
			case LogLevelTrace: return "trace";
			case LogLevelDebug: return "debug";
			case LogLevelInfo: return "info";
			case LogLevelWarn: return "warn";
			case LogLevelError: return "error";
			case LogLevelCritical: return "critical";
			case LogLevelOff: return "off";
			default: return "<UNKNOWN LOG LEVEL>";
		}
	}

	switch(level) {

		case LogLevelTrace: return WHITE BOLD "trace" NC;
		case LogLevelDebug: return LBLUE BOLD "debug" NC;
		case LogLevelInfo: return BLUE BOLD "info" NC;
		case LogLevelWarn: return YELLOW BOLD "warn" NC;
		case LogLevelError: return RED BOLD "error" NC;
		case LogLevelCritical: return MAGENTA BOLD "critical" NC;
		case LogLevelOff: return DEFAULT BOLD "off" NC;
		default: return "<UNKNOWN LOG LEVEL>";
	}
}

const char* __get_thread_name(void) {

	if(__log_thread_state.name) {
		return __log_thread_state.name;
	}

	pid_t tid = gettid();

	char* name;
	formatString(&name, "%d", tid);

	__log_thread_state.name = name;

	return __log_thread_state.name;
}

void __log_lock_mutex(void) {
	int result = pthread_mutex_lock(&__global_log_entry.mutex);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to lock the mutex for the logger");
}

void __log_unlock_mutex(void) {
	int result = pthread_mutex_unlock(&__global_log_entry.mutex);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to unlock the mutex for the logger");
}

void initialize_logger(void) {
	__global_log_entry.log_level = DEFAULT_LOG_LEVEL;

	int result = pthread_mutex_init(&__global_log_entry.mutex, NULL);
	checkResultForThreadErrorAndExit(
	    "An Error occurred while trying to initialize the mutex for the logger");
}

void set_log_level(LogLevel level) {
	__global_log_entry.log_level = level;
}

void set_thread_name(const char* name) {
	__log_thread_state.name = name;
}

int parse_log_level(const char* level) {

	if(strcmp(level, "trace") == 0) {
		return LogLevelTrace;
	} else if(strcmp(level, "debug") == 0) {
		return LogLevelDebug;
	} else if(strcmp(level, "info") == 0) {
		return LogLevelInfo;
	} else if(strcmp(level, "warn") == 0) {
		return LogLevelWarn;
	} else if(strcmp(level, "error") == 0) {
		return LogLevelError;
	} else if(strcmp(level, "critical") == 0) {
		return LogLevelCritical;
	} else if(strcmp(level, "off") == 0) {
		return LogLevelOff;
	}

	return -1;
}

const char* get_level_name(LogLevel level) {
	return __get_level_name(level, false);
}
