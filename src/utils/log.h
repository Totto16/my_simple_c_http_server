
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/utils.h"
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	LogLevelTrace = 0x00,
	LogLevelDebug = 0x01,
	LogLevelInfo = 0x02,
	LogLevelWarn = 0x03,
	LogLevelError = 0x04,
	LogLevelCritical = 0x05,
	LogLevelOff = 0x06,
} LogLevel;

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	LogPrintLocation = 0x08,
} LogFlags;

// only for internal use!!

NODISCARD bool should_log(LogLevel level);

NODISCARD bool should_log_to_stderr(LogLevel level);

NODISCARD const char* get_level_name_internal(LogLevel level, bool color);

NODISCARD const char* get_thread_name(void);

void log_lock_mutex(void);

void log_unlock_mutex(void);

NODISCARD bool log_should_use_color(void);

NODISCARD bool has_flag(int flags, LogFlags needle);

typedef struct {
	LogLevel level;
	LogFlags flags;
} LevelAndFlags;

LevelAndFlags get_level_and_flags(int level_and_flags);

#define LOG_MESSAGE(level_and_flags, msg, ...) \
	do { \
		LevelAndFlags destructured = get_level_and_flags(level_and_flags); \
		LogLevel level = destructured.level; \
		int flags = destructured.flags; \
		if(should_log(level)) { \
			bool should_use_color = log_should_use_color(); \
			const char* level_name = get_level_name_internal(level, should_use_color); \
			const char* thread_name = get_thread_name(); \
			log_lock_mutex(); \
			FILE* file_stream = should_log_to_stderr(level) ? stderr : stdout; \
			fprintf(file_stream, "[%s] ", level_name); \
			if(should_use_color) { \
				fprintf(file_stream, "[\033[32m%s\033[0m] ", thread_name); /*GREEN*/ \
			} else { \
				fprintf(file_stream, "[%s] ", thread_name); \
			} \
			if(has_flag(flags, LogPrintLocation)) { \
				fprintf(file_stream, "[%s %s:%d] ", __func__, __FILE__, __LINE__); \
			} \
			fprintf(file_stream, msg, __VA_ARGS__); \
			log_unlock_mutex(); \
		} \
	} while(false);

#define LOG_MESSAGE_SIMPLE(level_and_flags, msg) LOG_MESSAGE(level_and_flags, msg "%s", "")

// everybody can use them

// NOT thread safe
void initialize_logger(void);

// NOT thread safe
void set_log_level(LogLevel level);

// IS thread safe
void set_thread_name(const char* name);

// IS thread safe
void unset_thread_name(void);

// IS thread safe
NODISCARD int parse_log_level(const char* level);

NODISCARD const char* get_level_name(LogLevel level);

#ifdef __cplusplus
}
#endif
