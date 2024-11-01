/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#pragma once

// Note -D_POSIX_C_SOURCE -D_BSD_SOURCE are needed feature flags ONLY for ZID-DPL, on
// other more modern Systems these might throw a warning, but they're needed for older Systems!

#include <netinet/ip.h>
#include <poll.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

// all headers that are needed, so modular dependencies can be solved easily and also some "topics"
// stay in the same file
#include "generic/secure.h"
#include "http/http_protocol.h"
#include "utils/thread_pool.h"

// some general utils used in more programs, so saved into header!
#include "utils/utils.h"

// specific numbers for the task, these are arbitrary, but suited for that problem

#define SOCKET_BACKLOG_SIZE 10

#define INITIAL_MESSAGE_BUF_SIZE 1024

#define MAX_QUEUE_SIZE 100

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
char* readStringFromConnection(const ConnectionDescriptor* const descriptor);

enum REQUEST_SUPPORT_STATUS {
	REQUEST_SUPPORTED = 0,
	REQUEST_INVALID_HTTP_VERSION = 1,
	REQUEST_METHOD_NOT_SUPPORTED = 2,
	REQUEST_INVALID_NONEMPTY_BODY = 3,
};

// returns wether the protocol, method is supported, atm only GET and HTTP 1.1 are supported, if
// returned an enum state, the caller has to handle errors
int isRequestSupported(HttpRequest* request);

// structs for the listenerThread

typedef struct {
	thread_pool* pool;
	myqueue* jobIds;
	ConnectionContext* const* contexts;
	int socketFd;
} ThreadArgument;

typedef struct {
	ConnectionContext* const* contexts;
	pthread_t listenerThread;
	int connectionFd;
} ConnectionArgument;

typedef enum { JobErrorCode_DESC } JobErrorCode;

typedef struct {
	JobErrorCode error_code;
} JobError;

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

JobResult connectionHandler(job_arg arg, WorkerInfo workerInfo);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(NULL) threadFunction(anyType(ThreadArgument*) arg);

int startServer(uint16_t port, SecureOptions* const options);

void print_job_error(FILE* file, const JobError* const error);
void free_job_error(JobError* error);
