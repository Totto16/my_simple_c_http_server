

#pragma once

#include <unistd.h>

#include "./utils.h"

#ifdef __APPLE__

	#include <stdint.h>

typedef uint64_t ThreadIdType;
	#define PRI_THREADID "%llu"
	#define THREAD_ID_FORMATTED_MAX_SIZE 20
#else

typedef pid_t ThreadIdType;
	#define PRI_THREADID "%d"
	#define THREAD_ID_FORMATTED_MAX_SIZE 11
#endif

NODISCARD ThreadIdType get_thread_id(void);
