
#include "./tqueue.h"
#include "utils/log.h"

int tqueue_init(TQueue* queue) {
	int result = comp_sem_init(&(queue->can_access), 1, false);
	CHECK_FOR_ERROR(result, "Couldn't initialize the internal queue Semaphore", return -1;);

	TQueueHead* q_head = &(queue->head);
	STAILQ_INIT(q_head);
	queue->size = 0;
	return 0;
}

int tqueue_destroy(TQueue* queue) {
	// to clean up, the mutex has to be destroyed
	int result = comp_sem_destroy(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't destroy the internal queue Semaphore", return -1;);
	return 0;
}

bool tqueue_is_empty(TQueue* queue) {
	int result = comp_sem_wait(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal queue Semaphore", return false;);

	TQueueHead* q_head = &(queue->head);
	bool empty = STAILQ_EMPTY(q_head);
	if(empty != (queue->size == 0)) { // NOLINT(readability-implicit-bool-conversion)
		LOG_MESSAGE_SIMPLE(LogLevelCritical, "internal size implementation error in the queue!");
	}

	// now say that it can be accessed
	result = comp_sem_post(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't post the internal queue Semaphore", return false;);
	return empty;
}

// not checked for error code of malloc :(
// modified to use void * instead of int as stored value
int tqueue_push(TQueue* queue, void* value) {

	int result = comp_sem_wait(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal queue Semaphore", return -1;);

	TQueueHead* q_head = &(queue->head);
	TQueueEntry* entry = (TQueueEntry*)malloc(sizeof(TQueueEntry));
	entry->value = value;
	STAILQ_INSERT_TAIL(q_head, entry, entries);

	++(queue->size);

	// now say that it can be accessed
	result = comp_sem_post(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't post the internal queue Semaphore", return -1;);

	return 0;
}

void* tqueue_pop(TQueue* queue) {

	int result = comp_sem_wait(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal queue Semaphore", return NULL);

	TQueueHead* q_head = &(queue->head);
	// would be a deadlock, due to also waiting on the semaphore
	//	assert(!tqueue_is_empty(q_head));
	bool empty = STAILQ_EMPTY(q_head);
	assert(!empty && "The queue was empty on pop!");
	TQueueEntry* entry = STAILQ_FIRST(q_head);
	void* value = entry->value;
	STAILQ_REMOVE_HEAD(q_head, entries);
	free(entry);

	--(queue->size);

	// now say that it can be accessed
	result = comp_sem_post(&(queue->can_access));
	CHECK_FOR_ERROR(result, "Couldn't post the internal queue Semaphore", return NULL;);
	return value;
}
