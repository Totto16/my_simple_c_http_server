#pragma once

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "generic/sem.h"
#include "myqueue.h"
#include "utils.h"

#define THREAD_SHUTDOWN_JOB_INTERNAL 0x02

// defining the Shutdown Macro
#define THREAD_SHUTDOWN_JOB ((JobFunction)THREAD_SHUTDOWN_JOB_INTERNAL)

// defining the type defs

typedef struct {
	size_t worker_index;
} WorkerInfo;

typedef ANY_TYPE(JobResult*) (*JobFunction)(ANY_TYPE(UserType*), WorkerInfo);

typedef struct {
	pthread_t thread;
	// used to have more information, therefore a struct, it doesn'T take more memory, so its fine
} MyThreadPoolThreadInformation;

typedef struct {
	size_t worker_threads_amount;
	Myqueue jobqueue;
	MyThreadPoolThreadInformation* worker_threads;
	SemaphoreType jobs_available;
} ThreadPool;

typedef struct JobIdImpl JobId;

typedef struct {
	MyThreadPoolThreadInformation* information;
	ThreadPool* thread_pool;
	WorkerInfo worker_info;
} MyThreadPoolThreadArgument;

// this function is used internally as worker thread Function, therefore the rather cryptic name
// it handles all the submitted jobs, it wait for them with a semaphore, that is thread safe, and
// callable from different threads.
// it reads from the queue and then executes the job, and then marks it as complete (posting the job
// semaphore)
NODISCARD ANY_TYPE(NULL)
    thread_pool_worker_thread_function(ANY_TYPE(MyThreadPoolThreadArgument*) arg);

// creates a pool, the size denotes the size of the worker threads, if you don't know how to choose
// this value, use pool_create_dynamic to have an adjusted value, to your running system, it
// determines the right amount of threads to use in the CURRENTLY running system, that is
// recommended, since then this pool is more efficient, on every system
// pool is a address of an already declared, either mallcoed or on the stack (please ensure the
// lifetime is sufficient) thread_pool
NODISCARD int pool_create(ThreadPool* pool, size_t size);

// using get_nprocs_conf to make a dynamic amount of worker Threads
// returns the used dynamic thread amount, to use it in some way (maybe print it)
// this does the same as the pool_create method, but is recommended, since it calculates the worker
// threads on the fly, so it's better suited for every system, and no hardcoded worker threads are
// required!
NODISCARD int pool_create_dynamic(ThreadPool* pool);

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its NULL) so it is checked here and
// printing a warning if its _THREAD_SHUTDOWN_JOB and returns NULL
NODISCARD JobId* pool_submit(ThreadPool* pool, JobFunction start_routine, ANY_TYPE(UserType*) arg);

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its NULL) so it is checked here and
// printing a warning if its _THREAD_SHUTDOWN_JOB
NODISCARD ANY_TYPE(JobResult*) pool_await(JobId* job_description);

// destroys the thread_pool, has to be called AFTER all jobs where awaited, otherwise it'S undefined
// behaviour! this cn also block, until all jobs are finished
NODISCARD int pool_destroy(ThreadPool* pool);
