#pragma once

#include <netinet/ip.h>
#include <poll.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

// all headers that are needed, so modular dependencies can be solved easily and also some "topics"
// stay in the same file
#include "./routes.h"
#include "generic/authentication.h"
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
	RequestSupported = 0,
	RequestInvalidHttpVersion,
	RequestMethodNotSupported,
	RequestInvalidNonemptyBody,
} RequestSupportStatus;

// returns wether the protocol, method is supported, atm only GET and HTTP 1.1 are supported, if
// returned an enum state, the caller has to handle errors
NODISCARD RequestSupportStatus is_request_supported(HttpRequest* request);

// structs for the listenerThread

typedef struct {
	ThreadPool* pool;
	Myqueue* job_ids;
	ConnectionContextPtrs contexts;
	int socket_fd;
	WebSocketThreadManager* web_socket_manager;
	const RouteManager* route_manager;
	LifecycleFunctions fns;
} HTTPThreadArgument;

typedef struct {
	ConnectionContextPtrs contexts;
	pthread_t listener_thread;
	int connection_fd;
	WebSocketThreadManager* web_socket_manager;
	const RouteManager* route_manager;
} HTTPConnectionArgument;

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

NODISCARD ANY_TYPE(JobError*)
    http_socket_connection_handler(ANY_TYPE(HTTPConnectionArgument*) arg_ign,
                                   WorkerInfo worker_info);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
NODISCARD ANY_TYPE(NULL) http_listener_thread_function(ANY_TYPE(HTTPThreadArgument*) arg);

NODISCARD int start_http_server(uint16_t port, SecureOptions* options,
                                AuthenticationProviders* auth_providers, HTTPRoutes* routes);

void global_initialize_http_global_data(void);

void global_free_http_global_data(void);
