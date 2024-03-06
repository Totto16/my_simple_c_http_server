
#include "myqueue.h"

void myqueue_init(myqueue* q) {
	int result = sem_init(&(q->canAcces), -1, 1);
	checkResultForErrorAndExit("Couldn't initialize the internal queue Semaphore");

	myqueue_head* q_head = &(q->head);
	STAILQ_INIT(q_head);
	q->size = 0;
}

void myqueue_destroy(myqueue* q) {
	// to clean up, the mutex has to be destroyed
	int result = sem_destroy(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't destroy the internal queue Semaphore");
}

bool myqueue_is_empty(myqueue* q) {
	int result = sem_wait(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't wait for the internal queue Semaphore");

	myqueue_head* q_head = &(q->head);
	bool empty = STAILQ_EMPTY(q_head);
	if(empty != (q->size == 0)) {
		fprintf(stderr, "FATAL: internal size implementation error in the queue!");
	}

	// now say that it can be accessed
	result = sem_post(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't post the internal queue Semaphore");
	return empty;
}

// not checked for error code of malloc :(
// modified to use void * instead of int as stored value
void myqueue_push(myqueue* q, void* value) {

	int result = sem_wait(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't wait for the internal queue Semaphore");

	myqueue_head* q_head = &(q->head);
	struct myqueue_entry* entry = (struct myqueue_entry*)malloc(sizeof(struct myqueue_entry));
	entry->value = value;
	STAILQ_INSERT_TAIL(q_head, entry, entries);

	++(q->size);

	// now say that it can be accessed
	result = sem_post(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't post the internal queue Semaphore");
}

void* myqueue_pop(myqueue* q) {

	int result = sem_wait(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't wait for the internal queue Semaphore");

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
	result = sem_post(&(q->canAcces));
	checkResultForErrorAndExit("Couldn't post the internal queue Semaphore");
	return value;
}
