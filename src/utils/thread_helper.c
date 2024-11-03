

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>

#include "thread_helper.h"

pid_t get_thread_id(void) {
	return gettid();
}
