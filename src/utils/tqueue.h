
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

// taken from previous lecture, added the internal semaphore (not mutex, since they are not
// lock/unlockable from different threads (they can be with an attr, but thats not supported
// everywhere) ), so that it is thread-safe
// you don't have do to anything when callling the queue manipulation functions, they're
// synchronized on themeself

typedef struct TQueueEntryImpl TQueueEntry;

struct TQueueEntryImpl {
	GenericData value;
	STAILQ_ENTRY(TQueueEntryImpl) entries;
};

typedef struct TQueueHeadImpl TQueueHead;

STAILQ_HEAD(TQueueHeadImpl, TQueueEntryImpl);

typedef struct {
	TQueueHead head;
	SemaphoreType can_access;
	size_t size;
} TQueue;

NODISCARD GenericResult tqueue_init(TQueue* queue);

NODISCARD GenericResult tqueue_destroy(TQueue* queue);

NODISCARD bool tqueue_is_empty(TQueue* queue);

//NOTE: use trtti or tvec.h non generic push!

// not checked for error code of malloc :(
// modified to use void * instead of int as stored value
NODISCARD GenericResult tqueue_push(TQueue* queue,GenericData value);

NODISCARD GenericData tqueue_pop(TQueue* queue);
