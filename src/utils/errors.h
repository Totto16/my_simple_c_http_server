
#pragma once

#include "utils/utils.h"

#include <trtti.h>

// job errors

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint64_t) {
	JobErrorNone = 0x02,
	// is used to communicate something
	JobErrorConnectionUpgrade = 0x04,
	//
	JobErrorDesc = 0x20,
	JobErrorThreadCancel = 0x21,
	JobErrorMalloc = 0x22,
	JobErrorClose = 0x23,
	JobErrorStringFormat = 0x24,
	JobErrorInvalidJob = 0x25,
	JobErrorNoResult = 0x26,
	JobErrorSemWait = 0x27,
	JobErrorSemDest = 0x28,
	JobErrorSigHandler = 0x29,
	JobErrorGetSockName = 0x2A,
	JobErrorConnectionAdd = 0x2B,
	JobErrorCleanupConnection = 0x2C,
	JobErrorGeneric = 0x2D,

	JobErrorStart = JobErrorDesc,
	JobErrorEnd = JobErrorGeneric,
} JobError;

static_assert(sizeof(uint64_t) == sizeof(uint8_t*));

static_assert(sizeof(GenericData) == sizeof(uint8_t*));

NODISCARD bool is_job_error(ANY_TYPE(JobError) error);

void print_job_error(JobError error);

// listeners errors

typedef ANY ListenerError;

#define LISTENER_ERROR_NONE ((ListenerError)0x02)

#define LISTENER_ERROR_MALLOC ((ListenerError)0x80)
#define LISTENER_ERROR_THREAD_CANCEL ((ListenerError)0x81)
#define LISTENER_ERROR_QUEUE_PUSH ((ListenerError)0x82)
#define LISTENER_ERROR_ACCEPT ((ListenerError)0x83)
#define LISTENER_ERROR_DATA_CONTROLLER ((ListenerError)0x84)
#define LISTENER_ERROR_THREAD_AFTER_CANCEL ((ListenerError)0x85)

#define LISTENER_ERROR_START LISTENER_ERROR_MALLOC
#define LISTENER_ERROR_END LISTENER_ERROR_THREAD_AFTER_CANCEL

NODISCARD bool is_listener_error(ListenerError error);

void print_listener_error(ListenerError error);

// Create errors

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	CreateErrorNone = 0,
	CreateErrorThreadCreate,
	CreateErrorMalloc,
	CreateErrorSemInit,
	CreateErrorQueueInit
} CreateError;

void print_create_error(CreateError error);

// submit errors

typedef ANY SubmitError;

#define SUBMIT_ERROR_NONE ((SubmitError)0x02)

#define SUBMIT_ERROR_MALLOC ((SubmitError)0xA0)
#define SUBMIT_ERROR_SEM_INIT ((SubmitError)0xA1)
#define SUBMIT_ERROR_SEM_POST ((SubmitError)0xA2)
#define SUBMIT_ERROR_INVALID_START_ROUTINE ((SubmitError)0xA3)
#define SUBMIT_ERROR_QUEUE_PUSH ((SubmitError)0xA4)

void print_submit_error(SubmitError error);

// worker errors

typedef ANY WorkerError;

#define WORKER_ERROR_NONE ((WorkerError)0x02)

#define WORKER_ERROR_SEM_POST ((WorkerError)0xC0)
#define WORKER_ERROR_SEM_WAIT ((WorkerError)0xC1)

void print_worker_error(WorkerError error);
