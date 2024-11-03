
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

const char* __get_level_name(LogLevel level, bool color);

const char* __get_thread_name(void);

void __log_lock_mutex(void);

void __log_unlock_mutex(void);

bool __log_should_use_color(void);

#define LOG_MESSAGE(level, msg, ...) \
	do { \
		if(__should_log(level)) { \
			bool should_use_color = __log_should_use_color(); \
			const char* level_name = __get_level_name(level, should_use_color); \
			const char* thread_name = __get_thread_name(); \
			__log_lock_mutex(); \
			const char* thread_color = ""; \
			const char* thread_end = ""; \
			if(should_use_color) { \
				thread_color = "\033[32m"; /*GREEN*/ \
				thread_end = "\033[0m";    /*NC*/ \
			} \
			if(__should_log_to_stderr(level)) { \
				printf("[%s] [%s%s%s] " msg, level_name, thread_color, thread_name, thread_end, \
				       __VA_ARGS__); \
			} else { \
				fprintf(stderr, "[%s] [%s%s%s] " msg, level_name, thread_color, thread_name, \
				        thread_end, __VA_ARGS__); \
			} \
			__log_unlock_mutex(); \
		} \
	} while(false);

#define LOG_MESSAGE_SIMPLE(level, msg) LOG_MESSAGE(level, msg "%s", "")

// everybody can use them

// NOT thread safe
void initialize_logger(void);

// NOT thread safe
void set_log_level(LogLevel level);

// IS thread safe
void set_thread_name(const char* name);

// NOTE :this isn't freed at the end
#define SET_THREAD_NAME_FORMATTED(name, ...) \
	do { \
		char* result = NULL; \
		formatString(&result, name, __VA_ARGS__); \
		set_thread_name(result); \
	} while(false)

// IS thread safe
int parse_log_level(const char* level);

const char* get_level_name(LogLevel level);
