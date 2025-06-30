
#include "myqueue.h"
#include "utils/log.h"

int myqueue_init(myqueue* q) {
	int result = comp_sem_init(&(q->canAccess), 1, false);
	CHECK_FOR_ERROR(result, "Couldn't initialize the internal queue Semaphore", return -1;);

	myqueue_head* q_head = &(q->head);
	STAILQ_INIT(q_head);
	q->size = 0;
	return 0;
}

int myqueue_destroy(myqueue* q) {
	// to clean up, the mutex has to be destroyed
	int result = comp_sem_destroy(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't destroy the internal queue Semaphore", return -1;);
	return 0;
}

bool myqueue_is_empty(myqueue* q) {
	int result = comp_sem_wait(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal queue Semaphore", return false;);

	myqueue_head* q_head = &(q->head);
	bool empty = STAILQ_EMPTY(q_head);
	if(empty != (q->size == 0)) {
		fprintf(stderr, "FATAL: internal size implementation error in the queue!");
	}

	// now say that it can be accessed
	result = comp_sem_post(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't post the internal queue Semaphore", return false;);
	return empty;
}

// not checked for error code of malloc :(
// modified to use void * instead of int as stored value
int myqueue_push(myqueue* q, void* value) {

	int result = comp_sem_wait(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal queue Semaphore", return -1;);

	myqueue_head* q_head = &(q->head);
	struct myqueue_entry* entry = (struct myqueue_entry*)malloc(sizeof(struct myqueue_entry));
	entry->value = value;
	STAILQ_INSERT_TAIL(q_head, entry, entries);

	++(q->size);

	// now say that it can be accessed
	result = comp_sem_post(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't post the internal queue Semaphore", return -1;);

	return 0;
}

void* myqueue_pop(myqueue* q) {

	int result = comp_sem_wait(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal queue Semaphore", return NULL);

	myqueue_head* q_head = &(q->head);
	// would be a deadlock, due to also waiting on the semaphore
	//	assert(!myqueue_is_empty(q_head));
	bool empty = STAILQ_EMPTY(q_head);
	assert(!empty && "The queue was empty on pop!");
	struct myqueue_entry* entry = STAILQ_FIRST(q_head);
	void* value = entry->value;
	STAILQ_REMOVE_HEAD(q_head, entries);
	free(entry);

	--(q->size);

	// now say that it can be accessed
	result = comp_sem_post(&(q->canAccess));
	CHECK_FOR_ERROR(result, "Couldn't post the internal queue Semaphore", return NULL;);
	return value;
}
