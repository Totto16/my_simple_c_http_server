
#pragma once

#include <stdbool.h>

#ifdef _USE_BSD_QUEUE
#include "bsd/sys/queue.h"
#else
#include <sys/queue.h>
#endif

#include "generic/sem.h"

// in here there are several utilities that are used across all .h and .c files
#include "./utils.h"

// taken from previous lecture, added the internal semaphore (not mutex, since they ar not
// lock/unlockable from different threads (they can be with an attr, but thats not supported
// everywhere) ), so that it is thread-safe
// you don't have do to anything when callling the queue manipulation functions, they're
// synchronized on themeself

typedef struct MyqueueEntryImpl MyqueueEntry;

struct MyqueueEntryImpl {
	void* value;
	STAILQ_ENTRY(MyqueueEntryImpl) entries;
};

typedef struct MyqueueHeadImpl MyqueueHead;

STAILQ_HEAD(MyqueueHeadImpl, MyqueueEntryImpl);

typedef struct {
	MyqueueHead head;
	SemaphoreType can_access;
	int size;
} Myqueue;

NODISCARD int myqueue_init(Myqueue* queue);

NODISCARD int myqueue_destroy(Myqueue* queue);

NODISCARD bool myqueue_is_empty(Myqueue* queue);

// not checked for error code of malloc :(
// modified to use void * instead of int as stored value
NODISCARD int myqueue_push(Myqueue* queue, void* value);

NODISCARD void* myqueue_pop(Myqueue* queue);
