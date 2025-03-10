
#define _GNU_SOURCE
#include <pthread.h>
#undef _GNU_SOURCE

#include "log.h"
#include "thread_helper.h"
#include "utils/utils.h"

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

bool should_log(LogLevel level) {
	if(__global_log_entry.log_level == LogLevelOff) {
		return false;
	}

	return __global_log_entry.log_level <= level;
}

bool should_log_to_stderr(LogLevel level) {
	return level >= LogLevelError;
}

bool log_should_use_color(void) {
	return isatty(STDIN_FILENO);
}

bool has_flag(int flags, LogFlags needle) {
	return (flags & needle) != 0;
}

static const int level_flag_mask = 0b111;

LevelAndFlags get_level_and_flags(int level_and_flags) {
	int level = level_and_flags & level_flag_mask;
	int flags = level_and_flags & (~(level_flag_mask));

	return (LevelAndFlags){ .level = level, .flags = flags };
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

const char* get_level_name_internal(LogLevel level, bool color) {

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

const char* get_thread_name(void) {

	if(__log_thread_state.name) {
		return __log_thread_state.name;
	}

	pid_t tid = get_thread_id();

	char* name = NULL;
	formatString(
	    &name,
	    {
		    const char* fallback_name = "<failed setting thread name>";
		    __log_thread_state.name = fallback_name;
		    return fallback_name;
	    },
	    "TID %d", tid);

	__log_thread_state.name = name;

	return __log_thread_state.name;
}

void log_lock_mutex(void) {
	int result = pthread_mutex_lock(&__global_log_entry.mutex);
	checkForThreadError(result, "An Error occurred while trying to lock the mutex for the logger",
	                    return;);
}

void log_unlock_mutex(void) {
	int result = pthread_mutex_unlock(&__global_log_entry.mutex);
	checkForThreadError(result, "An Error occurred while trying to unlock the mutex for the logger",
	                    return;);
}

void initialize_logger(void) {
	__global_log_entry.log_level = DEFAULT_LOG_LEVEL;

	int result = pthread_mutex_init(&__global_log_entry.mutex, NULL);
	checkForThreadError(
	    result, "An Error occurred while trying to initialize the mutex for the logger", return;);
}

void set_log_level(LogLevel level) {
	__global_log_entry.log_level = level;
}

// taken from my work in oopetris
// inspired by SDL_SYS_SetupThread also uses that code for most platforms
void set_platform_thread_name(const char* name) {

#if defined(__APPLE__) || defined(__MACOSX__)
	if(pthread_setname_np(name) == ERANGE) {
		char namebuf[16] = {}; /* Limited to 16 chars (with 0 byte) */
		memcpy(namebuf, name, 15);
		namebuf[15] = '\0';
		pthread_setname_np(namebuf);
	}
#elif defined(__linux__) || defined(__ANDROID__)
	if(pthread_setname_np(pthread_self(), name) == ERANGE) {
		char namebuf[16] = {}; /* Limited to 16 chars (with 0 byte) */
		memcpy(namebuf, name, 15);
		namebuf[15] = '\0';
		pthread_setname_np(pthread_self(), namebuf);
	}
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

	SetThreadDescription(GetCurrentThread(), name);

#else
	UNUSED(name);
#endif
}

void set_thread_name(const char* name) {
	__log_thread_state.name = name;
	set_platform_thread_name(name);
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
	return get_level_name_internal(level, false);
}
