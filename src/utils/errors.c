

#include "errors.h"

#include <stdlib.h>

#include "utils/log.h"

bool is_job_error(ANY_TYPE(JobError) error) {
	return (JobError)error == JobErrorNone || // NOLINT(readability-implicit-bool-conversion)
	       (JobError)error == JobErrorConnectionUpgrade ||
	       ((JobError)error >= JobErrorStart && (JobError)error <= JobErrorEnd);
}

void print_job_error(JobError error) {

	const char* error_str = "Unknown error";

	if(error == JobErrorNone) {
		error_str = "None";
	} else if(error == JobErrorConnectionUpgrade) {
		error_str = "Connection Upgrade";
	} else if(error == JobErrorDesc) {
		error_str = "Desc";
	} else if(error == JobErrorThreadCancel) {
		error_str = "ThreadCancel";
	} else if(error == JobErrorMalloc) {
		error_str = "Malloc";
	} else if(error == JobErrorClose) {
		error_str = "Close";
	} else if(error == JobErrorStringFormat) {
		error_str = "StringFormat";
	} else if(error == JobErrorInvalidJob) {
		error_str = "InvalidJob";
	} else if(error == JobErrorNoResult) {
		error_str = "NoResult";
	} else if(error == JobErrorSemWait) {
		error_str = "SemWait";
	} else if(error == JobErrorSemDest) {
		error_str = "SemDest";
	} else if(error == JobErrorSigHandler) {
		error_str = "SigHandler";
	} else if(error == JobErrorGetSockName) {
		error_str = "GetSockName";
	} else if(error == JobErrorConnectionAdd) {
		error_str = "ConnectionAdd";
	} else if(error == JobErrorCleanupConnection) {
		error_str = "CleanupConnection";
	} else if(error == JobErrorGeneric) {
		error_str = "Generic";
	}

	LOG_MESSAGE(LogLevelError, "Job Error: %s\n", error_str);
}

bool is_listener_error(ListenerError error) {
	return error == LISTENER_ERROR_NONE || // NOLINT(readability-implicit-bool-conversion)
	       (error >= LISTENER_ERROR_START && error <= LISTENER_ERROR_END);
}

void print_listener_error(ListenerError error) {

	const char* error_str = "Unknown error";

	if(error == LISTENER_ERROR_NONE) {
		error_str = "None";
	} else if(error == LISTENER_ERROR_MALLOC) {
		error_str = "Malloc";
	} else if(error == LISTENER_ERROR_THREAD_CANCEL) {
		error_str = "ThreadCancel";
	} else if(error == LISTENER_ERROR_QUEUE_PUSH) {
		error_str = "QueuePush";
	} else if(error == LISTENER_ERROR_ACCEPT) {
		error_str = "Accept";
	} else if(error == LISTENER_ERROR_DATA_CONTROLLER) {
		error_str = "DataController";
	} else if(error == LISTENER_ERROR_THREAD_AFTER_CANCEL) {
		error_str = "ThreadAfterCancel";
	}

	LOG_MESSAGE(LogLevelError, "Listener Error: %s\n", error_str);
}

void print_create_error(const CreateError error) {

	const char* error_str = "Unknown error"; // NOLINT(clang-analyzer-deadcode.DeadStores)
	switch(error) {
		case CreateErrorNone: error_str = "None"; break;
		case CreateErrorThreadCreate: error_str = "ThreadCreate"; break;
		case CreateErrorMalloc: error_str = "Malloc"; break;
		case CreateErrorSemInit: error_str = "SemInit"; break;
		case CreateErrorQueueInit: error_str = "QueueInit"; break;
		default: break;
	}

	LOG_MESSAGE(LogLevelError, "Create Error: %s\n", error_str);
}

void print_submit_error(SubmitError error) {

	const char* error_str = "Unknown error";

	if(error == SUBMIT_ERROR_NONE) {
		error_str = "None";
	} else if(error == SUBMIT_ERROR_MALLOC) {
		error_str = "Malloc";
	} else if(error == SUBMIT_ERROR_SEM_INIT) {
		error_str = "SemInit";
	} else if(error == SUBMIT_ERROR_SEM_POST) {
		error_str = "SemPost";
	} else if(error == SUBMIT_ERROR_INVALID_START_ROUTINE) {
		error_str = "InvalidStartRoutine";
	} else if(error == SUBMIT_ERROR_QUEUE_PUSH) {
		error_str = "QueuePush";
	}

	LOG_MESSAGE(LogLevelError, "Submit Error: %s\n", error_str);
}

void print_worker_error(WorkerError error) {

	const char* error_str = "Unknown error";

	if(error == WORKER_ERROR_NONE) {
		error_str = "None";
	} else if(error == WORKER_ERROR_SEM_POST) {
		error_str = "SemPost";
	} else if(error == WORKER_ERROR_SEM_WAIT) {
		error_str = "SemWait";
	}

	LOG_MESSAGE(LogLevelError, "Worker Error: %s\n", error_str);
}
