
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
	//
	JobErrorStart = JobErrorDesc,
	JobErrorEnd = JobErrorGeneric,
} JobError;

static_assert(sizeof(uint64_t) == sizeof(uint8_t*));

static_assert(sizeof(GenericData) == sizeof(uint8_t*));

NODISCARD bool is_job_error(ANY_TYPE(JobError) error);

void print_job_error(JobError error);

// listeners errors

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint64_t) {
	ListenerErrorNone = 0x02,
	//
	ListenerErrorMalloc = 0x80,
	ListenerErrorThreadCancel = 0x81,
	ListenerErrorQueuePush = 0x82,
	ListenerErrorAccept = 0x83,
	ListenerErrorDataController = 0x84,
	ListenerErrorThreadAfterCancel = 0x85,
	ListenerErrorGeneric = 0x86,
	//
	ListenerErrorStart = ListenerErrorMalloc,
	ListenerErrorEnd = ListenerErrorGeneric,
} ListenerError;

NODISCARD bool is_listener_error(ANY_TYPE(ListenerError) error);

void print_listener_error(ListenerError error);

// Create errors

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	CreateErrorNone = 0,
	//
	CreateErrorThreadCreate,
	CreateErrorMalloc,
	CreateErrorSemInit,
	CreateErrorQueueInit
} CreateError;

void print_create_error(CreateError error);

// submit errors

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint64_t) {
	SubmitErrorNone = 0x02,
	//
	SubmitErrorMalloc = 0xA0,
	SubmitErrorSemInit = 0xA1,
	SubmitErrorSemPost = 0xA2,
	SubmitErrorInvalidStartRoutine = 0xA3,
	SubmitErrorQueuePush = 0xA4,
} SubmitError;

void print_submit_error(SubmitError error);

// worker errors

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint64_t) {
	WorkerErrorNone = 0x02,
	//
	WorkerErrorSemPost = 0xC0,
	WorkerErrorSemWait = 0xC1,
} WorkerError;

void print_worker_error(WorkerError error);
