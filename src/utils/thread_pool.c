
#include "thread_pool.h"
#include "generic/hash.h"
#include "generic/helper.h"
#include "utils/log.h"

#define THREAD_SHUTDOWN_JOB_INTERNAL 0x02

// defining the Shutdown Macro
#define THREAD_SHUTDOWN_JOB ((JobFunction)THREAD_SHUTDOWN_JOB_INTERNAL)

struct JobIdImpl {
	SemaphoreType status;
	JobFunction job_function;
	ANY_TYPE(JobArgument) argument;
	ANY_TYPE(JobResult) result;
};

static void thread_pool_worker_thread_startup_function(void) {
#ifdef _SIMPLE_SERVER_USE_OPENSSL
	openssl_initialize_crypto_thread_state();
#endif

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Running startup function for http thread\n");
}

static void thread_pool_worker_thread_shutdown_function(void) {
#ifdef _SIMPLE_SERVER_USE_OPENSSL
	openssl_cleanup_crypto_thread_state();
#endif

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Running shutdown function for http thread\n");
}

// this function is used internally as worker thread Function, therefore the rather cryptic name
// it handles all the submitted jobs, it wait for them with a semaphore, that is thread safe, and
// callable from different threads.
// it reads from the queue and then executes the job, and then marks it as complete (posting the job
// semaphore)
ANY_TYPE(NULL)
thread_pool_worker_thread_function(ANY_TYPE(my_thread_pool_ThreadArgument*) const arg) {
	// casting it to the given element, (arg) is a malloced struct, so it has to be freed at the end
	// of that function!
	const MyThreadPoolThreadArgument argument = *((const MyThreadPoolThreadArgument* const)arg);
	// extracting the queue for later use
	TQueue* const jobs_queue = &(argument.thread_pool->job_queue);

	RUN_LIFECYCLE_FN(argument.thread_pool->fns.startup_fn);

	// looping until receiving the shutdown signal, to know more about that, read pool_destroy
	while(true) {
		// block here until a job is available and can be worked upon
		const LibCInt result = comp_sem_wait(&(argument.thread_pool->jobs_available));
		CHECK_FOR_ERROR(result, "Couldn't wait for the internal thread pool Semaphore",
		                return WORKER_ERROR_SEM_WAIT;);

		// that here is an assert, but it'S that important, so that I wrote it without assertions,
		// since there is a way to  disable assertions!
		if(tqueue_is_empty(jobs_queue)) {
			LOG_MESSAGE_SIMPLE(LogLevelError,
			                   "Expected to have elements in the queue at this stage in internal "
			                   "thread pool implementation, but got nothing!\n");
			// TODO(Totto): don't exit here
			exit(ExitCodeFailure);
		}

		// getting the job from the queue, the queue is synchronized INTERNALLY!
		JobId* const current_job = (JobId*)tqueue_pop(jobs_queue);

		// when receiving shutdown signal, It breaks out of the while loop and finsishes
		if(current_job->job_function == THREAD_SHUTDOWN_JOB) {
			RUN_LIFECYCLE_FN(argument.thread_pool->fns.shutdown_fn);

			// to be able to await for this job too, it has to post the sempahore before leaving!
			const LibCInt result2 = comp_sem_post(&(current_job->status));
			CHECK_FOR_ERROR(result2,
			                "Couldn't post the internal thread pool Semaphore for a single job",
			                return WORKER_ERROR_SEM_POST;);
			break;
		}

		// otherwise it just calls the function, and therefore executes it
		ANY_TYPE(JobResult)
		return_value = current_job->job_function(current_job->argument, argument.worker_info);
		// atm a warning issued, when a functions returns something other than NULL, but thats
		// only there, to show that it doesn't get returned, it wouldn't be that big of a deal to
		// implement this, but it isn't needed and required
		current_job->result = return_value;

		// finally cleaning up by posting the semaphore
		const LibCInt result3 = comp_sem_post(&(current_job->status));
		CHECK_FOR_ERROR(result3,
		                "Couldn't post the internal thread pool Semaphore for a single job",
		                return WORKER_ERROR_SEM_POST;);
	}

	// was malloced and not freed elsewhere, so it gets freed after use
	free(arg);
	// nothing to return, so NULL is returned
	return WORKER_ERROR_NONE;
}

// using get_nprocs_conf to make a dynamic amount of worker Threads
// returns the used dynamic thread amount, to use it in some way (maybe print it)
// this does the same as the pool_create method, but is recommended, since it calculates the worker
// threads on the fly, so it's better suited for every system, and no hardcoded worker threads are
// required!
static CreateResult pool_create_dynamic(ThreadPool* const pool) { // NOLINT(misc-no-recursion)
	// can't fail according to man pages
	const size_t active_cpu_cores = get_active_cpu_cores();

	if(active_cpu_cores == 0) {
		return (CreateResult){ .error = CreateErrorQueueInit };
	}

	// + 1 since not all threads run all the time, so the extra one thread is used for compensating
	// the idle time of a core
	const size_t worker_threads_amount = active_cpu_cores + 1;

	// the just calling pool create with that number
	const CreateResult result = pool_create(pool, worker_threads_amount);
	if(result.error != CreateErrorNone) {
		return result;
	}

	return (CreateResult){ .error = CreateErrorNone, .value = { .size = worker_threads_amount } };
}

// creates a pool, the size denotes the size of the worker threads, if you don't know how to choose
// this value, use pool_create_dynamic to have an adjusted value, to your running system, it
// determines the right amount of threads to use in the CURRENTLY running system, that is
// recommended, since then this pool is more efficient, on every system
// pool is a address of an already declared, either malloced or on the stack (please ensure the
// lifetime is sufficient) thread_pool
CreateResult pool_create(ThreadPool* const pool, const size_t size) { // NOLINT(misc-no-recursion)
	if(size == 0) {
		return pool_create_dynamic(pool);
	}

	// writing the values to the struct
	pool->worker_threads_amount = size;
	// allocating the worker Threads array, they are freed in destroy!
	pool->worker_threads =
	    (MyThreadPoolThreadInformation*)malloc(sizeof(MyThreadPoolThreadInformation) * size);

	pool->fns = (LifecycleFunctions){ .startup_fn = thread_pool_worker_thread_startup_function,
		                              .shutdown_fn = thread_pool_worker_thread_shutdown_function };

	if(!pool->worker_threads) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return (CreateResult){ .error = CreateErrorMalloc };
	}

	// initialize the queue, this queue is synchronized internally, so it has to do some work with a
	// synchronization method (here not necessary to know how it's implemented, but it'S a
	// semaphore)
	const GenericResult init_res = tqueue_init(&(pool->job_queue));

	IF_GENERIC_RESULT_IS_ERROR_IGN(init_res) {
		return (CreateResult){ .error = CreateErrorQueueInit };
	}

	// now initialize the thread jobs_available sempahore, it denotes how many jobs are in the
	// queue, so that a worker thread can get one from the queue and work upon that job pshared i 0,
	// since it'S shared between threads!
	const LibCInt result = comp_sem_init(&(pool->jobs_available), 0, true);
	CHECK_FOR_ERROR(result, "Couldn't initialize the internal thread pool Semaphore",
	                return (CreateResult){ .error = CreateErrorSemInit };);

	for(size_t i = 0; i < size; i++) {
		// doing a malloc for every single one, so that it can be freed after the threads is
		// finished, here a struct, that is allocated on the stack wouldn't have a lifetime that is
		// suited for that use case, after the for loop it's "dead", unusable
		MyThreadPoolThreadArgument* thread_argument =
		    (MyThreadPoolThreadArgument*)malloc(sizeof(MyThreadPoolThreadArgument));

		if(!thread_argument) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
			                   "Couldn't allocate memory!\n");
			return (CreateResult){ .error = CreateErrorMalloc };
		}

		// initializing the struct with the necessary values
		thread_argument->information = &(pool->worker_threads[i]);
		thread_argument->worker_info.worker_index = i;
		thread_argument->thread_pool = pool;
		// now launch the worker thread

		const LibCInt result2 = pthread_create(&((pool->worker_threads[i]).thread), NULL,
		                                       thread_pool_worker_thread_function, thread_argument);
		CHECK_FOR_THREAD_ERROR(result2,
		                       "An Error occurred while trying to create a new Worker "
		                       "Thread in the implementation of thread pool",
		                       return (CreateResult){ .error = CreateErrorThreadCreate });
	}

	return (CreateResult){ .error = CreateErrorNone, .value = { .size = size } };
}

// submits a function with argument to the job queue, returns a job_id struct, that HAS to be used
// later to await this job,
//  if you don't save this in some way, you could have serious problems, you need to pool_await
//  every job_id before calling pool_destroy
// otherwise the behaviour is undefined!
// the function argument has to be malloced or on a stack with enough lifetime, the pointer to it
// has to be valid until pool_await is called!
static JobId* int_pool_submit(ThreadPool* pool, JobFunction start_routine, ANY_TYPE(JobArg) arg) {
	JobId* job_description = (JobId*)malloc(sizeof(JobId));

	if(!job_description) {
		return SUBMIT_ERROR_MALLOC;
	}

	// initializing the struct
	job_description->argument = arg;
	job_description->job_function = start_routine;
	job_description->result = JOB_ERROR_NO_RESULT;

	// initializing with 0, it gets posted after the job was proccessed by a worker!!
	// pshared i 0, since it'S shared between threads!
	const LibCInt result = comp_sem_init(&(job_description->status), 0, true);
	CHECK_FOR_ERROR(result,
	                "Couldn't initialize the internal thread pool Semaphore for a single job",
	                return SUBMIT_ERROR_SEM_INIT;);
	// then finally push the job to the queue, so it can worked upon
	const GenericResult push_res = tqueue_push(&(pool->job_queue), job_description);
	IF_GENERIC_RESULT_IS_ERROR_IGN(push_res) {
		return SUBMIT_ERROR_QUEUE_PUSH;
	}
	// after the push the semaphore gets posted, so a worker can get the job already, if available
	const LibCInt result2 = comp_sem_post(&(pool->jobs_available));
	CHECK_FOR_ERROR(result2, "Couldn't post the internal thread pool Semaphore",
	                return SUBMIT_ERROR_SEM_POST);

	// finally return the job_id struct, it's malloced, so it has to be freed later! (that is done
	// by the pool_await!)
	return job_description;
}

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its an invalid function pointer) so it is
// checked here and printing a warning if its _THREAD_SHUTDOWN_JOB and returns a SubmitError
JobId* pool_submit(ThreadPool* pool, JobFunction start_routine, ANY_TYPE(JobArg) arg) {
	if(start_routine != THREAD_SHUTDOWN_JOB) {
		return int_pool_submit(pool, start_routine, arg);
	}

	LOG_MESSAGE_SIMPLE(LogLevelWarn, "invalid job_function passed to pool_submit!\n");
	return SUBMIT_ERROR_INVALID_START_ROUTINE;
}

// if a job is not awaited, its memory is NOT freed, and some other problems occur, so ALWAYS await
// it! It is undefined behaviour if not all jobs are awaited before calling pool_destroy
// this function can block, and waits until the job is finished, a semaphore is used for that
// also don't manipulate the properties of that struct, only pass it to the adequate function,
// otherwise undefined behaviour might occur!
// after calling this function the content of the job_id is garbage, since it'S free, if you have a
// copy, DON'T use it, it is undefined what happens when using this already freed chunk of memory
static ANY_TYPE(JobResult) impl_pool_await(JobId* const job_description) {
	// wait for the internal semaphore, that can block
	LibCInt result = comp_sem_wait(&(job_description->status));
	CHECK_FOR_ERROR(result, "Couldn't wait for the internal thread pool Semaphore for a single job",
	                return JOB_ERROR_SEM_WAIT;);

	// then finally destroy the semaphore, it isn't used anymore
	result = comp_sem_destroy(&(job_description->status));
	CHECK_FOR_ERROR(result, "Couldn't destroy the internal thread pool Semaphore",
	                return JOB_ERROR_SEM_DEST;);

	OOM_ASSERT(job_description != (void*)THREAD_SHUTDOWN_JOB_INTERNAL, "error description");

	ANY_TYPE(JobResult)
	const job_result =
	    job_description->result; // NOLINT(clang-analyzer-core.FixedAddressDereference)

	// finally free the allocated job_id
	free(job_description); // NOLINT(clang-analyzer-unix.Malloc)

	return job_result;
}

// visible to the user, checks for "invalid" input before invoking the inner "real" function!
// _THREAD_SHUTDOWN_JOB can't be delivered by the user! (its an invalid function pointer) so it is
// checked here and printing a warning if its _THREAD_SHUTDOWN_JOB
ANY_TYPE(JobResult) pool_await(JobId* job_description) {
	if(job_description != (void*)THREAD_SHUTDOWN_JOB_INTERNAL) {
		return impl_pool_await(job_description);
	}

	LOG_MESSAGE_SIMPLE(LogLevelError, "WARNING: invalid job_function passed to pool_submit!\n");
	return JOB_ERROR_INVALID_JOB;
}

// destroys the thread_pool, has to be called AFTER all jobs where awaited, otherwise it'S undefined
// behaviour! this cn also block, until all jobs are finished
GenericResult pool_destroy(ThreadPool* const pool) {

	// first set shutdown Flag to true for all, then afterwards check if they did, (or are waiting
	// for the semaphore to increment)
	// using smart method to shutdown, each worker thread receives a _THREAD_SHUTDOWN_JOB job, that
	// is handled as macro NULL and therefore it can signal the shutdown, the behavior when calling
	// pool destroy is as stated: each thread receives the shutdown job, if jobs get submitted after
	// destroy, they DON'T get worked upon, and also it is shutdown after ALL remaining jobs
	// are finished, so it's only well defined, if waited upon all jobs!
	for(size_t i = 0; i < pool->worker_threads_amount; ++i) {
		impl_pool_await(int_pool_submit(pool, THREAD_SHUTDOWN_JOB, NULL));
	}

	// then finally join all the worker threads, this is done after sending a shutdown signal, so
	// that it is already executed before calling join, if not it just blocks a littel amount of
	// time, nothing to bad can happen
	for(size_t i = 0; i < pool->worker_threads_amount; ++i) {
		const LibCInt result = pthread_join(pool->worker_threads[i].thread, NULL);
		CHECK_FOR_THREAD_ERROR(result,
		                       "An Error occurred while trying to wait for a Worker "
		                       "Thread in the implementation of thread pool",
		                       return GENERIC_RES_ERR_UNIQUE(););
	}

	// free the struct allocated by pool_create
	free(pool->worker_threads);

	// destroy the queue!
	const GenericResult destroy_result = tqueue_destroy(&(pool->job_queue));

	IF_GENERIC_RESULT_IS_ERROR_IGN(destroy_result) {
		return destroy_result;
	}

	// and the finally the semaphore, that is responsible for the jobs
	const LibCInt result = comp_sem_destroy(&(pool->jobs_available));
	CHECK_FOR_ERROR(result, "Couldn't destroy the internal thread pool Semaphore",
	                return GENERIC_RES_ERR_UNIQUE(););

	return GENERIC_RES_OK();
}
