#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "myqueue.h"
#include "utils.h"

#define __THREAD_SHUTDOWN_JOB_INTERNAL 0x02

// defining the Shutdown Macro
#define _THREAD_SHUTDOWN_JOB ((job_function)__THREAD_SHUTDOWN_JOB_INTERNAL)

// defining the type defs

typedef struct {
	size_t workerIndex;
} WorkerInfo;

typedef anyType(JobResult*) (*job_function)(anyType(UserType*), WorkerInfo);

typedef struct {
	pthread_t thread;
	// used to have more information, therefore a struct, it doesn'T take more memory, so its fine
} _my_thread_pool_ThreadInformation;

typedef struct {
	size_t workerThreadAmount;
	myqueue jobqueue;
	_my_thread_pool_ThreadInformation* workerThreads;
	sem_t jobsAvailable;
} thread_pool;

typedef struct job_id_impl job_id;

typedef struct {
	_my_thread_pool_ThreadInformation* information;
	thread_pool* threadPool;
	WorkerInfo workerInfo;
} _my_thread_pool_ThreadArgument;

// this function is used internally as worker thread Function, therefore the rather cryptic name
// it handles all the submitted jobs, it wait for them with a semaphore, that is thread safe, and
// callable from different threads.
// it reads from the queue and then executes the job, and then marks it as complete (posting the job
// semaphore)
anyType(NULL) _thread_pool_Worker_thread_function(anyType(_my_thread_pool_ThreadArgument*) arg);

// creates a pool, the size denotes the size of the worker threads, if you don't know how to choose
// this value, use pool_create_dynamic to have an adjusted value, to your running system, it
// determines the right amount of threads to use in the CURRENTLY running system, that is
// recommended, since then this pool is more efficient, on every system
// pool is a address of an already declared, either mallcoed or on the stack (please ensure the
// lifetime is sufficient) thread_pool
int pool_create(thread_pool* pool, size_t size);

// using get_nprocs_conf to make a dynamic amount of worker Threads
// returns the used dynamic thread amount, to use it in some way (maybe print it)
// this does the same as the pool_create method, but is recommended, since it calculates the worker
// threads on the fly, so it's better suited for every system, and no hardcoded worker threads are
// required!
int pool_create_dynamic(thread_pool* pool);

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its NULL) so it is checked here and
// printing a warning if its _THREAD_SHUTDOWN_JOB and returns NULL
job_id* pool_submit(thread_pool* pool, job_function start_routine, anyType(USerType*) arg);

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its NULL) so it is checked here and
// printing a warning if its _THREAD_SHUTDOWN_JOB
[[nodiscard]] anyType(JobResult*) pool_await(job_id* jobDescription);

// destroys the thread_pool, has to be called AFTER all jobs where awaited, otherwise it'S undefined
// behaviour! this cn also block, until all jobs are finished
[[nodiscard]] int pool_destroy(thread_pool* pool);
