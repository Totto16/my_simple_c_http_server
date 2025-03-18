

#pragma once

#include <unistd.h>

#ifdef __APPLE__

#include <stdint.h>

typedef uint64_t THREAD_ID_TYPE;
#define PRI_THREADID "%llu"
#else

typedef pid_t THREAD_ID_TYPE;
#define PRI_THREADID "%d"
#endif

THREAD_ID_TYPE get_thread_id(void);
