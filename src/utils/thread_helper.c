

#define _GNU_SOURCE
#include <unistd.h>
#undef _GNU_SOURCE

#include "thread_helper.h"

pid_t get_thread_id(void) {
	return gettid();
}
