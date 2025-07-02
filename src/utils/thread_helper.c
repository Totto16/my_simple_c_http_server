

#define _GNU_SOURCE // NOLINT(readability-identifier-naming,bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#include <unistd.h>
#undef _GNU_SOURCE

#ifdef __APPLE__
#include <pthread.h>
#endif

#include "thread_helper.h"

ThreadIdType get_thread_id(void) {

#ifdef __APPLE__
	// See https://elliotth.blogspot.com/2012/04/gettid-on-mac-os.html
	// and https://man.freebsd.org/cgi/man.cgi?query=pthread_getthreadid_np&sektion=3&format=html
	uint64_t tid;
	pthread_threadid_np(NULL, &tid);
	return tid;
#else
	return gettid();

#endif
}
