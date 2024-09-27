/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#pragma once

// Note -D_POSIX_C_SOURCE -D_BSD_SOURCE are needed feature flags ONLY for ZID-DPL, on
// other more modern Systems these might throw a warning, but they're needed for older Systems!

#include <errno.h>
#include <netinet/ip.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

// all headers that are needed, so modular dependencies can be solved easily and also some "topics"
// stay in the same file
#include "http_protocol.h"
#include "secure.h"
#include "string_builder.h"
#include "thread_pool.h"

// some general utils used in more programs, so saved into header!
#include "utils.h"

// specific numbers for the task, these are arbitrary, but suited for that problem

#define SOCKET_BACKLOG_SIZE 10

#define INITIAL_MESSAGE_BUF_SIZE 1024

#define MAX_QUEUE_SIZE 100

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
char* readStringFromConnection(const ConnectionDescriptor* const descriptor);

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
void sendStringToConnection(const ConnectionDescriptor* const descriptor, char* toSend);

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
void sendStringBuilderToConnection(const ConnectionDescriptor* const descriptor,
                                   StringBuilder* stringBuilder);

void sendMallocedMessageToConnectionWithHeaders(const ConnectionDescriptor* const descriptor,
                                                int status, char* body, char const* MIMEType,
                                                HttpHeaderField* headerFields,
                                                const int headerFieldsAmount);

// sends a http message to the connection, takes status and if that special status needs some
// special headers adds them, mimetype can be NULL, then default one is used, see http_protocol.h
// for more
void sendMallocedMessageToConnection(const ConnectionDescriptor* const descriptor, int status,
                                     char* body, char const* MIMEType);

// same as above, but with unmalloced content, like char const* indicates
void sendMessageToConnection(const ConnectionDescriptor* const descriptor, int status,
                             char const* body, char const* MIMEType);

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

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

ignoredJobResult connectionHandler(job_arg arg, WorkerInfo workerInfo);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(NULL) threadFunction(anyType(ThreadArgument*) arg);

int startServer(uint16_t port, SecureOptions* const options);
