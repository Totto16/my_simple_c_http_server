
#pragma once

typedef enum {
	LogLevelTrace = 0,
	LogLevelDebug,
	LogLevelInfo,
	LogLevelWarn,
	LogLevelError,
	LogLevelCritical,
	LogLevelOff
} LogLevel;

// only for internal use!!

bool __should_log(LogLevel level);

bool __should_log_to_stderr(LogLevel level);

const char* __get_level_name(LogLevel level);

const char* __get_thread_name();

void __log_lock_mutex();

void __log_unlock_mutex();

#define LOG_MESSAGE(level, msg, ...) \
	do { \
		if(__should_log(level)) { \
			const char* level_name = __get_level_name(level); \
			const char* thread_name = __get_thread_name(); \
			__log_lock_mutex(); \
			if(__should_log_to_stderr(level)) { \
				printf("[%s] [%s] " msg, level_name, thread_name, __VA_ARGS__); \
			} else { \
				fprintf(stderr, "[%s] [%s] " msg, level_name, thread_name, __VA_ARGS__); \
			} \
			__log_unlock_mutex(); \
		} \
	} while(false);

// everybody can use them

// NOT thread safe
void initialize_logger();

// NOT thread safe
void set_log_level(LogLevel level);

// IS thread safe
void set_thread_name(const char* name);

// IS thread safe
int parse_log_level(const char* level);
