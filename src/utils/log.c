
#define _GNU_SOURCE // NOLINT(readability-identifier-naming,bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
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

static GlobalLogState
    g_global_value_log_entry = { // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
	    .log_level = DEFAULT_LOG_LEVEL
    };

// thread state

typedef struct {
	const char* name;
} ThreadState;

static _Thread_local ThreadState
    g_global_value_log_thread_state = { // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
	    .name = NULL
    };

bool log_should_log(LogLevel level) {
	if(g_global_value_log_entry.log_level == LogLevelOff) {
		return false;
	}

	return g_global_value_log_entry.log_level <= level;
}

bool log_should_log_to_stderr(LogLevel level) {
	return level >= LogLevelError;
}

bool log_should_use_color(bool stderr) {
	return isatty(stderr ? STDERR_FILENO : STDOUT_FILENO) != 0;
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

// 4 for "TID " and 12 for a bit of additional buffer so that the static message can be
// fitted
#define ADDITIONAL_TID_SIZE (4 + 12)

#define THREAD_LOCAL_STORAGE_FALLBACK_BUFF_SIZE (THREAD_ID_FORMATTED_MAX_SIZE + ADDITIONAL_TID_SIZE)

static _Thread_local char
    g_thread_local_name_storage_fallback // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        [THREAD_LOCAL_STORAGE_FALLBACK_BUFF_SIZE] = "<Unknown TID / name value>";

const char* get_thread_name(void) {

	if(g_global_value_log_thread_state.name) {
		return g_global_value_log_thread_state.name;
	}

	char* casted_static_buffer = (char*)g_thread_local_name_storage_fallback;

	g_global_value_log_thread_state.name = casted_static_buffer;

	ThreadIdType tid = get_thread_id();

	int written = snprintf(casted_static_buffer, THREAD_LOCAL_STORAGE_FALLBACK_BUFF_SIZE,
	                       "TID " PRI_THREADID, tid);
	if(written >= THREAD_LOCAL_STORAGE_FALLBACK_BUFF_SIZE) {
		return g_global_value_log_thread_state.name;
	}

	return g_global_value_log_thread_state.name;
}

void log_lock_mutex(void) {
	int result = pthread_mutex_lock(&g_global_value_log_entry.mutex);

	if(result != 0) {
		/*pthread function don't set errno, but return the error value \
		 * directly*/
		fprintf(stderr, "An Error occurred while trying to lock the mutex for the logger: %s\n",
		        strerror(result));
		exit(EXIT_FAILURE);
	}
}

void log_unlock_mutex(void) {
	int result = pthread_mutex_unlock(&g_global_value_log_entry.mutex);

	if(result != 0) {
		/*pthread function don't set errno, but return the error value \
		 * directly*/
		fprintf(stderr, "An Error occurred while trying to unlock the mutex for the logger: %s\n",
		        strerror(result));
		exit(EXIT_FAILURE);
	}
}

void initialize_logger(void) {
	g_global_value_log_entry.log_level = DEFAULT_LOG_LEVEL;

	int result = pthread_mutex_init(&g_global_value_log_entry.mutex, NULL);
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to initialize the mutex for the logger", return;);
}

void set_log_level(LogLevel level) {
	g_global_value_log_entry.log_level = level;
}

// taken from my work in oopetris
// inspired by SDL_SYS_SetupThread also uses that code for most platforms
static void set_platform_thread_name(const char* name) {

#define NAME_BUF_MAX_SIZE 16

#if defined(__APPLE__) || defined(__MACOSX__)
	if(pthread_setname_np(name) == ERANGE) {
		char namebuf[NAME_BUF_MAX_SIZE] = {}; /* Limited to 16 chars (with 0 byte) */
		memcpy(namebuf, name, NAME_BUF_MAX_SIZE - 1);
		namebuf[NAME_BUF_MAX_SIZE - 1] = '\0';
		pthread_setname_np(namebuf);
	}
#elif defined(__linux__) || defined(__ANDROID__)
	if(pthread_setname_np(pthread_self(), name) == ERANGE) {
		char namebuf[NAME_BUF_MAX_SIZE] = {}; /* Limited to 16 chars (with 0 byte) */
		memcpy(namebuf, name, NAME_BUF_MAX_SIZE - 1);
		namebuf[NAME_BUF_MAX_SIZE - 1] = '\0';
		pthread_setname_np(pthread_self(), namebuf);
	}
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

	SetThreadDescription(GetCurrentThread(), name);

#else
	UNUSED(name);
#endif
}

void set_thread_name(const char* name) {
	g_global_value_log_thread_state.name = name;
	set_platform_thread_name(name);
}

void unset_thread_name(void) {
	g_global_value_log_thread_state.name = NULL;
}

int parse_log_level(const char* level) {

	if(strcmp(level, "trace") == 0) {
		return LogLevelTrace;
	}
	if(strcmp(level, "debug") == 0) {
		return LogLevelDebug;
	}
	if(strcmp(level, "info") == 0) {
		return LogLevelInfo;
	}
	if(strcmp(level, "warn") == 0) {
		return LogLevelWarn;
	}
	if(strcmp(level, "error") == 0) {
		return LogLevelError;
	}
	if(strcmp(level, "critical") == 0) {
		return LogLevelCritical;
	}
	if(strcmp(level, "off") == 0) {
		return LogLevelOff;
	}

	return -1;
}

const char* get_level_name(LogLevel level) {
	return get_level_name_internal(level, false);
}
