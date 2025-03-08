
#pragma once

// job errors

typedef void* JobError;

#define JobError_None ((JobError)0x02)

#define JobError_Desc ((JobError)0x20)
#define JobError_ThreadCancel ((JobError)0x21)
#define JobError_Malloc ((JobError)0x22)
#define JobError_Close ((JobError)0x23)
#define JobError_StringFormat ((JobError)0x24)
#define JobError_InvalidJob ((JobError)0x25)
#define JobError_NoResult ((JobError)0x26)
#define JobError_SemWait ((JobError)0x27)
#define JobError_SemDest ((JobError)0x28)
#define JobError_SigHandler ((JobError)0x29)

#define JobError_START JobError_Desc
#define JobError_END JobError_SigHandler

bool is_job_error(JobError error);

void print_job_error(JobError error);

// listeners errors

typedef void* ListenerError;

#define ListenerError_None ((ListenerError)0x02)

#define ListenerError_Malloc ((ListenerError)0x80)
#define ListenerError_ThreadCancel ((ListenerError)0x81)
#define ListenerError_QueuePush ((ListenerError)0x82)

#define ListenerError_START ListenerError_Malloc
#define ListenerError_END ListenerError_QueuePush


bool is_listener_error(ListenerError error);

void print_listener_error(ListenerError error);

// Create errors

/**
 * @enum value
 */
typedef enum {
	CreateError_None = 0,
	CreateError_ThreadCreate,
	CreateError_Malloc,
	CreateError_SemInit,
	CreateError_QueueInit
} CreateError;

void print_create_error(CreateError error);

// submit errors

typedef void* SubmitError;

#define SubmitError_None ((SubmitError)0x02)

#define SubmitError_Malloc ((SubmitError)0xA0)
#define SubmitError_SemInit ((SubmitError)0xA1)
#define SubmitError_SemPost ((SubmitError)0xA2)
#define SubmitError_InvalidStartRoutine ((SubmitError)0xA3)
#define SubmitError_QueuePush ((SubmitError)0xA4)

void print_submit_error(SubmitError error);

// worker errors

typedef void* WorkerError;

#define WorkerError_None ((WorkerError)0x02)

#define WorkerError_SemPost ((WorkerError)0xC0)
#define WorkerError_SemWait ((WorkerError)0xC1)

void print_worker_error(WorkerError error);
