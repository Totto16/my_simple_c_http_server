
#include "thread_pool.h"
#include "errors.h"
#include "utils/log.h"

#ifdef _DONT_HAVE_SYS_SYSINFO
#include <unistd.h>
#else
#include <sys/sysinfo.h>
#endif

struct job_id_impl {
	SEMAPHORE_TYPE status;
	job_function jobFunction;
	anyType(JobArgument) argument;
	anyType(JobResult) result;
};

// this function is used internally as worker thread Function, therefore the rather cryptic name
// it handles all the submitted jobs, it wait for them with a semaphore, that is thread safe, and
// callable from different threads.
// it reads from the queue and then executes the job, and then marks it as complete (posting the job
// semaphore)
anyType(NULL) thread_pool_worker_thread_function(anyType(my_thread_pool_ThreadArgument*) arg) {
	// casting it to the given element, (arg) is a malloced struct, so it has to be freed at the end
	// of that function!
	my_thread_pool_ThreadArgument argument = *((my_thread_pool_ThreadArgument*)arg);
	// extracting the queue for later use
	myqueue* JobsQueue = &(argument.threadPool->jobqueue);
	// looping until receiving the shutdown signal, to know more about that, read pool_destroy
	while(true) {

		// block here until a job is available and can be worked uppon
		int result = comp_sem_wait(&(argument.threadPool->jobsAvailable));
		checkForError(result, "Couldn't wait for the internal thread pool Semaphore",
		              return WorkerError_SemWait;);

		// that here is an assert, but it'S that important, so that I wrote it without assertions,
		// since there is a way to  disable assertions!
		if(myqueue_is_empty(JobsQueue)) {
			fprintf(stderr, "Expected to have elements in the queue at this stage in internal "
			                "thread pool implementation, but got nothing!\n");
			// TODO(Totto): don't exit here
			exit(EXIT_FAILURE);
		}

		// getting the job from the queue, the queue is synchronized INTERNALLY!
		job_id* currentJob = (job_id*)myqueue_pop(JobsQueue);

		// when receiving shutdown signal, It breaks out of the while loop and finsishes
		if(currentJob->jobFunction == THREAD_SHUTDOWN_JOB) {
			// to be able to await for this job toot, it has to post the sempahore before leaving!
			result = comp_sem_post(&(currentJob->status));
			checkForError(result,
			              "Couldn't post the internal thread pool Semaphore for a single job",
			              return WorkerError_SemPost;);
			break;
		}

		// otherwise it just calls the function, and therefore executes it
		anyType(JobResult) returnValue =
		    currentJob->jobFunction(currentJob->argument, argument.workerInfo);
		// atm a warning issued, when a functions returns something other than NULL, but thats
		// only there, to show that it doesn't get returned, it wouldn't be that big of a deal to
		// implement this, but it isn't needed and required
		currentJob->result = returnValue;

		// finally cleaning up by posting the semaphore
		result = comp_sem_post(&(currentJob->status));
		checkForError(result, "Couldn't post the internal thread pool Semaphore for a single job",
		              return WorkerError_SemPost;);
	}

	// was malloced and not freed elsewhere, so it gets freed after use
	free(arg);
	// nothing to return, so NULL is returned
	return WorkerError_None;
}

// creates a pool, the size denotes the size of the worker threads, if you don't know how to choose
// this value, use pool_create_dynamic to have an adjusted value, to your running system, it
// determines the right amount of threads to use in the CURRENTLY running system, that is
// recommended, since then this pool is more efficient, on every system
// pool is a address of an already declared, either malloced or on the stack (please ensure the
// lifetime is sufficient) thread_pool
int pool_create(thread_pool* pool, size_t size) {
	// writing the values to the struct
	pool->workerThreadAmount = size;
	// allocating the worker Threads array, they are freed in destroy!
	pool->workerThreads = (my_thread_pool_ThreadInformation*)mallocWithMemset(
	    sizeof(my_thread_pool_ThreadInformation) * size, true);

	if(!pool->workerThreads) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return CreateError_Malloc;
	}

	// initialize the queue, this queue is synchronized internally, so it has to do some work with a
	// synchronization method (here not necessary to know how it's implemented, but it'S a
	// semaphore)
	if(myqueue_init(&(pool->jobqueue)) < 0) {
		return CreateError_QueueInit;
	}

	// now initialize the thread jobsAvailable sempahore, it denotes how many jobs are in the queue,
	// so that a worker thread can get one from the queue and work upon that job
	// pshared i 0, since it'S shared between threads!
	int result = comp_sem_init(&(pool->jobsAvailable), 0, true);
	checkForError(result, "Couldn't initialize the internal thread pool Semaphore",
	              return CreateError_SemInit;);

	for(size_t i = 0; i < size; i++) {
		// doing a malloc for every single one, so that it can be freed after the threads is
		// finished, here a struct, that is allocated on the stack wouldn't have a lifetime that is
		// suited for that use case, after the for loop it's "dead", unusable
		my_thread_pool_ThreadArgument* threadArgument =
		    (my_thread_pool_ThreadArgument*)malloc(sizeof(my_thread_pool_ThreadArgument));

		if(!threadArgument) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return CreateError_Malloc;
		}

		// initializing the struct with the necessary values
		threadArgument->information = &(pool->workerThreads[i]);
		threadArgument->workerInfo.workerIndex = i;
		threadArgument->threadPool = pool;
		// now launch the worker thread
		result = pthread_create(&((pool->workerThreads[i]).thread), NULL,
		                        thread_pool_worker_thread_function, threadArgument);
		checkForThreadError(result,
		                    "An Error occurred while trying to create a new Worker "
		                    "Thread in the implementation of thread pool",
		                    return CreateError_ThreadCreate);
	}

	return CreateError_None;
}

int get_active_cpu_core(void) {
#ifdef _DONT_HAVE_SYS_SYSINFO
	// see https://www.unix.com/man_page/osx/3/sysconf
	return sysconf(_SC_NPROCESSORS_ONLN);
#else
	return get_nprocs();
#endif
}

// using get_nprocs_conf to make a dynamic amount of worker Threads
// returns the used dynamic thread amount, to use it in some way (maybe print it)
// this does the same as the pool_create method, but is recommended, since it calculates the worker
// threads on the fly, so it's better suited for every system, and no hardcoded worker threads are
// required!
int pool_create_dynamic(thread_pool* pool) {
	// can't fail according to man pages
	int activeCPUCores = get_active_cpu_core();
	// + 1 since not all threads run all the time, so the extra one thread is used for compensating
	// the idle time of a core
	size_t workerThreadsAmount = (size_t)activeCPUCores + 1;
	// the just calling pool create with that number
	int result = pool_create(pool, workerThreadsAmount);
	if(result != CreateError_None) {
		return -result;
	}

	return (int)workerThreadsAmount;
}

// submits a function with argument to the job queue, returns a job_id struct, that HAS to be used
// later to await this job,
//  if you don't save this in some way, you could have serious problems, you need to pool_await
//  every job_id before calling pool_destroy
// otherwise the behaviour is undefined!
// the function argument has to be malloced or on a stack with enough lifetime, the pointer to it
// has to be valid until pool_await is called!
static job_id* int_pool_submit(thread_pool* pool, job_function start_routine, anyType(JobArg) arg) {
	job_id* jobDescription = (job_id*)malloc(sizeof(job_id));

	if(!jobDescription) {
		return SubmitError_Malloc;
	}

	// initializing the struct
	jobDescription->argument = arg;
	jobDescription->jobFunction = start_routine;
	jobDescription->result = JobError_NoResult;

	// initializing with 0, it gets posted after the job was proccessed by a worker!!
	// pshared i 0, since it'S shared between threads!
	int result = comp_sem_init(&(jobDescription->status), 0, true);
	checkForError(result, "Couldn't initialize the internal thread pool Semaphore for a single job",
	              return SubmitError_SemInit;);
	// then finally push the job to the queue, so it can worked upon
	if(myqueue_push(&(pool->jobqueue), jobDescription) < 0) {
		return SubmitError_QueuePush;
	}
	// after the push the semaphore gets posted, so a worker can get the job already, if available
	result = comp_sem_post(&(pool->jobsAvailable));
	checkForError(result, "Couldn't post the internal thread pool Semaphore",
	              return SubmitError_SemPost);

	// finally return the job_id struct, it's malloced, so it has to be freed later! (that is done
	// by the pool_await!)
	return jobDescription;
}

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its an invalid function pointer) so it is
// checked here and printing a warning if its _THREAD_SHUTDOWN_JOB and returns a SubmitError
job_id* pool_submit(thread_pool* pool, job_function start_routine, anyType(JobArg) arg) {
	if(start_routine != THREAD_SHUTDOWN_JOB) {
		return int_pool_submit(pool, start_routine, arg);
	}

	LOG_MESSAGE_SIMPLE(LogLevelWarn, "invalid job_function passed to pool_submit!\n");
	return SubmitError_InvalidStartRoutine;
}

// if a job is not awaited, its memory is NOT freed, and some other problems occur, so ALWAYS await
// it! It is undefined behaviour if not all jobs are awaited before calling pool_destroy
// this function can block, and waits until the job is finished, a semaphore is used for that
// also don't manipulate the properties of that struct, only pass it to the adequate function,
// otherwise undefined behaviour might occur!
// after calling this function the content of the job_id is garbage, since it'S free, if you have a
// copy, DON'T use it, it is undefined what happens when using this already freed chunk of memory
static anyType(JobResult) impl_pool_await(job_id* jobDescription) {
	// wait for the internal semaphore, that can block
	int result = comp_sem_wait(&(jobDescription->status));
	checkForError(result, "Couldn't wait for the internal thread pool Semaphore for a single job",
	              return JobError_SemWait;);

	// then finally destroy the semaphore, it isn't used anymore
	result = comp_sem_destroy(&(jobDescription->status));
	checkForError(result, "Couldn't destroy the internal thread pool Semaphore",
	              return JobError_SemDest;);

	anyType(JobResult) job_result = jobDescription->result;

	// finally free the allocated job_id
	free(jobDescription); // NOLINT(clang-analyzer-unix.Malloc)

	return job_result;
}

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its an invalid function pointer) so it is
// checked here and printing a warning if its _THREAD_SHUTDOWN_JOB
anyType(JobResult) pool_await(job_id* jobDescription) {
	if(jobDescription != (void*)THREAD_SHUTDOWN_JOB_INTERNAL) {
		return impl_pool_await(jobDescription);
	}

	fprintf(stderr, "WARNING: invalid job_function passed to pool_submit!\n");
	return JobError_InvalidJob;
}

// destroys the thread_pool, has to be called AFTER all jobs where awaited, otherwise it'S undefined
// behaviour! this cn also block, until all jobs are finished
int pool_destroy(thread_pool* pool) {

	// first set shutdown Flag to true for all, then afterwards check if they did, (or are waiting
	// for the semaphore to increment)
	// using smart method to shutdown, each worker thread receives a _THREAD_SHUTDOWN_JOB job, that
	// is handled as macro NULL and therefore it can signal the shutdown, the behavior when calling
	// pool destroy is as stated: each thread receives the shutdown job, if jobs get submitted after
	// destroy, they DON'T get worked upon, and also it is shutdown after ALL remaining jobs
	// are finished, so it's only well defined, if waited upon all jobs!
	for(size_t i = 0; i < pool->workerThreadAmount; ++i) {
		impl_pool_await(int_pool_submit(pool, THREAD_SHUTDOWN_JOB, NULL));
	}

	// then finally join all the worker threads, this is done after sending a shutdown signal, so
	// that it is already executed before calling join, if not it just blocks a littel amount of
	// time, nothing to bad can happen
	for(size_t i = 0; i < pool->workerThreadAmount; ++i) {
		int result = pthread_join(pool->workerThreads[i].thread, NULL);
		checkForThreadError(result,
		                    "An Error occurred while trying to wait for a Worker "
		                    "Thread in the implementation of thread pool",
		                    return -2;);
	}

	// free the struct allocated by pool_create
	free(pool->workerThreads);

	// destroy the queue!
	if(myqueue_destroy(&(pool->jobqueue)) < 0) {
		return -1;
	}

	// and the finally the semaphore, that is responsible for the jobs
	int result = comp_sem_destroy(&(pool->jobsAvailable));
	checkForError(result, "Couldn't destroy the internal thread pool Semaphore", return -1;);

	return 0;
}
