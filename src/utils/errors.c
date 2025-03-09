

#include "errors.h"

#include <stdlib.h>

#include "utils/log.h"

bool is_job_error(const JobError error) {
	return error == JobError_None || (error >= JobError_START && error <= JobError_END);
}

void print_job_error(const JobError error) {

	const char* error_str = "Unknown error";

	if(error == JobError_None) {
		error_str = "None";
	} else if(error == JobError_Desc) {
		error_str = "Desc";
	} else if(error == JobError_ThreadCancel) {
		error_str = "ThreadCancel";
	} else if(error == JobError_Malloc) {
		error_str = "Malloc";
	} else if(error == JobError_Close) {
		error_str = "Close";
	} else if(error == JobError_StringFormat) {
		error_str = "StringFormat";
	} else if(error == JobError_InvalidJob) {
		error_str = "InvalidJob";
	} else if(error == JobError_NoResult) {
		error_str = "NoResult";
	} else if(error == JobError_SemWait) {
		error_str = "SemWait";
	} else if(error == JobError_SemDest) {
		error_str = "SemDest";
	} else if(error == JobError_SigHandler) {
		error_str = "SigHandler";
	} else if(error == JobError_GetSockName) {
		error_str = "GetSockName";
	}

	LOG_MESSAGE(LogLevelError, "Job Error: %s\n", error_str);
}

bool is_listener_error(const ListenerError error) {
	return error == ListenerError_None ||
	       (error >= ListenerError_START && error <= ListenerError_END);
}

void print_listener_error(const ListenerError error) {

	const char* error_str = "Unknown error";

	if(error == ListenerError_None) {
		error_str = "None";
	} else if(error == ListenerError_Malloc) {
		error_str = "Malloc";
	} else if(error == ListenerError_ThreadCancel) {
		error_str = "ThreadCancel";
	} else if(error == ListenerError_QueuePush) {
		error_str = "QueuePush";
	} else if(error == ListenerError_Accept) {
		error_str = "Accept";
	}else if(error == ListenerError_DataController) {
		error_str = "DataController";
	}

	LOG_MESSAGE(LogLevelError, "Listener Error: %s\n", error_str);
}

void print_create_error(const CreateError error) {

	const char* error_str = "Unknown error";
	switch(error) {
		case CreateError_None: error_str = "None"; break;
		case CreateError_ThreadCreate: error_str = "ThreadCreate"; break;
		case CreateError_Malloc: error_str = "Malloc"; break;
		case CreateError_SemInit: error_str = "SemInit"; break;
		case CreateError_QueueInit: error_str = "QueueInit"; break;
		default: break;
	}

	LOG_MESSAGE(LogLevelError, "Create Error: %s\n", error_str);
}

void print_submit_error(const SubmitError error) {

	const char* error_str = "Unknown error";

	if(error == SubmitError_None) {
		error_str = "None";
	} else if(error == SubmitError_Malloc) {
		error_str = "Malloc";
	} else if(error == SubmitError_SemInit) {
		error_str = "SemInit";
	} else if(error == SubmitError_SemPost) {
		error_str = "SemPost";
	} else if(error == SubmitError_InvalidStartRoutine) {
		error_str = "InvalidStartRoutine";
	} else if(error == SubmitError_QueuePush) {
		error_str = "QueuePush";
	}

	LOG_MESSAGE(LogLevelError, "Submit Error: %s\n", error_str);
}

void print_worker_error(const WorkerError error) {

	const char* error_str = "Unknown error";

	if(error == WorkerError_None) {
		error_str = "None";
	} else if(error == WorkerError_SemPost) {
		error_str = "SemPost";
	} else if(error == WorkerError_SemWait) {
		error_str = "SemWait";
	}

	LOG_MESSAGE(LogLevelError, "Worker Error: %s\n", error_str);
}
