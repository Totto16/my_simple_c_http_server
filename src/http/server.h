#pragma once

// Note -D_POSIX_C_SOURCE -D_BSD_SOURCE are needed feature flags ONLY for ZID-DPL, on
// other more modern Systems these might throw a warning, but they're needed for older Systems!

#include <netinet/ip.h>
#include <poll.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

// all headers that are needed, so modular dependencies can be solved easily and also some "topics"
// stay in the same file
#include "./routes.h"
#include "generic/secure.h"
#include "http/http_protocol.h"
#include "utils/thread_pool.h"
#include "ws/thread_manager.h"

// some general utils used in more programs, so saved into header!
#include "utils/utils.h"

// specific numbers for the task, these are arbitrary, but suited for that problem

#define HTTP_SOCKET_BACKLOG_SIZE 10

#define HTTP_MAX_QUEUE_SIZE 100

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	REQUEST_SUPPORTED = 0,
	REQUEST_INVALID_HTTP_VERSION,
	REQUEST_METHOD_NOT_SUPPORTED,
	REQUEST_INVALID_NONEMPTY_BODY,
} REQUEST_SUPPORT_STATUS;

// returns wether the protocol, method is supported, atm only GET and HTTP 1.1 are supported, if
// returned an enum state, the caller has to handle errors
int isRequestSupported(HttpRequest* request);

// structs for the listenerThread

typedef struct {
	thread_pool* pool;
	myqueue* jobIds;
	ConnectionContext** contexts;
	int socketFd;
	WebSocketThreadManager* webSocketManager;
	const RouteManager* routeManager;
} HTTPThreadArgument;

typedef struct {
	ConnectionContext** contexts;
	pthread_t listenerThread;
	int connectionFd;
	WebSocketThreadManager* webSocketManager;
	const RouteManager* routeManager;
} HTTPConnectionArgument;

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

anyType(JobError*)
    http_socket_connection_handler(anyType(HTTPConnectionArgument*) arg, WorkerInfo workerInfo);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(NULL) http_listener_thread_function(anyType(HTTPThreadArgument*) arg);

int startHttpServer(uint16_t port, SecureOptions* options);
