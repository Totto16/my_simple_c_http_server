

#pragma once

#include <unistd.h>

#include "./utils.h"

#ifdef __APPLE__

#include <stdint.h>

typedef uint64_t ThreadIdType;
#define PRI_THREADID "%llu"
#else

typedef pid_t ThreadIdType;
#define PRI_THREADID "%d"
#endif

NODISCARD ThreadIdType get_thread_id(void);
