

#pragma once

#include <unistd.h>

#ifdef __APPLE__

#include <stdlib.h>

typedef uint64_t THREAD_ID_TYPE;
#define PRI_THREADID "%lu"
#else
typedef pid_t THREAD_ID_TYPE;
#define PRI_THREADID "%d"
#endif

THREAD_ID_TYPE get_thread_id(void);
