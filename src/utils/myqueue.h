
#pragma once

#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

// in here there are several utilities that are used across all .h and .c files
#include "utils.h"

// taken from previous lecture, added the internal semaphore (not mutex, since they ar not
// lock/unlockable from different threads (they can be with an attr, but thats not supported
// everywhere) ), so that it is thread-safe
// you don't have do to anything when callling the queue manipulation functions, they're
// synchronized on themeself
struct myqueue_entry {
	void* value;
	STAILQ_ENTRY(myqueue_entry) entries;
};

STAILQ_HEAD(myqueue_head, myqueue_entry);

typedef struct myqueue_head myqueue_head;

typedef struct {
	myqueue_head head;
	sem_t canAccess;
	int size;
} myqueue;

[[nodiscard]] int myqueue_init(myqueue* q);

[[nodiscard]] int myqueue_destroy(myqueue* q);

bool myqueue_is_empty(myqueue* q);

// not checked for error code of malloc :(
// modified to use void * instead of int as stored value
[[nodiscard]] int myqueue_push(myqueue* q, void* value);

void* myqueue_pop(myqueue* q);
