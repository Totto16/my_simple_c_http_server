/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include <errno.h>
#include <signal.h>

#include "generic/read.h"
#include "generic/secure.h"
#include "generic/send.h"
#include "server.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/thread_pool.h"
#include "utils/utils.h"
#include "ws/handler.h"
#include "ws/thread_manager.h"
#include "ws/ws.h"

// returns wether the protocol, method is supported, atm only GET and HTTP 1.1 are supported, if
// returned an enum state, the caller has to handle errors
int isRequestSupported(HttpRequest* request) {
	if(strcmp(request->head.requestLine.protocolVersion, "HTTP/1.1") != 0) {
		return REQUEST_INVALID_HTTP_VERSION;
	} else if(strcmp(request->head.requestLine.method, "GET") != 0 &&
	          strcmp(request->head.requestLine.method, "POST") != 0 &&
	          strcmp(request->head.requestLine.method, "HEAD") != 0 &&
	          strcmp(request->head.requestLine.method, "OPTIONS") != 0) {
		return REQUEST_METHOD_NOT_SUPPORTED;
	} else if((strcmp(request->head.requestLine.method, "GET") == 0 ||
	           strcmp(request->head.requestLine.method, "HEAD") == 0 ||
	           strcmp(request->head.requestLine.method, "OPTIONS") == 0) &&
	          strlen(request->body) != 0) {
		return REQUEST_INVALID_NONEMPTY_BODY;
	}

	return REQUEST_SUPPORTED;
}

static volatile sig_atomic_t signal_received = 0;

// only setting the volatile sig_atomic_t signal_received' in here
static void receiveSignal(int signalNumber) {
	signal_received = signalNumber;
}

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

anyType(JobError*)
    __socket_connection_handler(anyType(ConnectionArgument*) _arg, WorkerInfo workerInfo) {

	// attention arg is malloced!
	ConnectionArgument* argument = (ConnectionArgument*)_arg;

	ConnectionContext* context = argument->contexts[workerInfo.workerIndex];
	char* thread_name_buffer = NULL;
	formatString(&thread_name_buffer, return JobError_StringFormat;
	             , "connection handler %lu", workerInfo.workerIndex);
	set_thread_name(thread_name_buffer);

#define FREE_AT_END() \
	do { \
		free(thread_name_buffer); \
		free(argument); \
	} while(false)

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting Connection handler\n");

	const ConnectionDescriptor* const descriptor =
	    get_connection_descriptor(context, argument->connectionFd);

	if(descriptor == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "get_connection_descriptor failed\n");

		return JobError_Desc;
	}

	char* rawHttpRequest = readStringFromConnection(descriptor);

	if(!rawHttpRequest) {
		int result =
		    sendMessageToConnection(descriptor, HTTP_STATUS_BAD_REQUEST,
		                            "Request couldn't be read, a connection error occurred!",
		                            MIME_TYPE_TEXT, NULL, 0, CONNECTION_SEND_FLAGS_UN_MALLOCED);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
		}

		goto cleanup;
	}

	// rawHttpRequest gets freed in here
	HttpRequest* httpRequest = parseHttpRequest(rawHttpRequest);

	// To test this error codes you can use '-X POST' with curl or
	// '--http2' (doesn't work, since http can only be HTTP/1.1, https can be HTTP 2 or QUIC
	// alias HTTP 3)

	// httpRequest can be null, then it wasn't parse-able, according to parseHttpRequest, see
	// there for more information
	if(httpRequest == NULL) {
		int result = sendMessageToConnection(
		    descriptor, HTTP_STATUS_BAD_REQUEST, "Request couldn't be parsed, it was malformed!",
		    MIME_TYPE_TEXT, NULL, 0, CONNECTION_SEND_FLAGS_UN_MALLOCED);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
		}

		goto cleanup;
	}

	// if the request is supported then the "beautiful" website is sent, if the URI is /shutdown
	// a shutdown is issued

	const int isSupported = isRequestSupported(httpRequest);

	if(isSupported == REQUEST_SUPPORTED) {
		if(strcmp(httpRequest->head.requestLine.method, "GET") == 0) {
			// HTTP GET
			if(strcmp(httpRequest->head.requestLine.URI, "/shutdown") == 0) {
				printf("Shutdown requested!\n");
				int result = sendMessageToConnection(descriptor, HTTP_STATUS_OK, "Shutting Down",
				                                     MIME_TYPE_TEXT, NULL, 0,
				                                     CONNECTION_SEND_FLAGS_UN_MALLOCED);

				if(result < 0) {
					LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
				}

				// just cancel the listener thread, then no new connection are accepted and the
				// main thread cleans the pool and queue, all jobs are finished so shutdown
				// gracefully
				int cancel_result = pthread_cancel(argument->listenerThread);
				checkForError(cancel_result, "While trying to cancel the listener Thread",
				              return JobError_ThreadCancel;);

			} else if(strcmp(httpRequest->head.requestLine.URI, "/") == 0) {
				int result = sendMessageToConnection(
				    descriptor, HTTP_STATUS_OK,
				    httpRequestToHtml(httpRequest, is_secure_context(context)), MIME_TYPE_HTML,
				    NULL, 0, CONNECTION_SEND_FLAGS_MALLOCED);

				if(result < 0) {
					LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
				}
			} else if(strcmp(httpRequest->head.requestLine.URI, "/ws") == 0) {
				int wsRequestSuccessful = handleWSHandshake(httpRequest, descriptor);

				if(wsRequestSuccessful >= 0) {
					// move the context so that we can use it in the long standing web socket
					// thread
					ConnectionContext* newContext = copy_connection_context(context);
					argument->contexts[workerInfo.workerIndex] = newContext;

					thread_manager_add_connection(argument->webSocketManager, descriptor, context,
					                              websocketFunction);

					// finally free everything necessary

					freeHttpRequest(httpRequest);
					FREE_AT_END();

					return JobError_None;
				}

				// the error was already sent, just close the descriptor and free the http
				// request, this is done at the end of this big if else statements

			} else if(strcmp(httpRequest->head.requestLine.URI, "/ws/fragmented") == 0) {
				int wsRequestSuccessful = handleWSHandshake(httpRequest, descriptor);

				if(wsRequestSuccessful >= 0) {
					// move the context so that we can use it in the long standing web socket
					// thread
					ConnectionContext* newContext = copy_connection_context(context);
					argument->contexts[workerInfo.workerIndex] = newContext;

					thread_manager_add_connection(argument->webSocketManager, descriptor, context,
					                              websocketFunctionFragmented);

					// finally free everything necessary

					freeHttpRequest(httpRequest);
					FREE_AT_END();

					return JobError_None;
				}

				// the error was already sent, just close the descriptor and free the http
				// request, this is done at the end of this big if else statements

			} else {
				int result = sendMessageToConnection(descriptor, HTTP_STATUS_NOT_FOUND,
				                                     "File not Found", MIME_TYPE_TEXT, NULL, 0,
				                                     CONNECTION_SEND_FLAGS_UN_MALLOCED);

				if(result < 0) {
					LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
				}
			}
		} else if(strcmp(httpRequest->head.requestLine.method, "POST") == 0) {
			// HTTP POST

			int result =
			    sendMessageToConnection(descriptor, HTTP_STATUS_OK,
			                            httpRequestToJSON(httpRequest, is_secure_context(context)),
			                            MIME_TYPE_JSON, NULL, 0, CONNECTION_SEND_FLAGS_MALLOCED);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
			}
		} else if(strcmp(httpRequest->head.requestLine.method, "HEAD") == 0) {
			// TODO send actual Content-Length, experiment with e.g a large video file!
			int result = sendMessageToConnection(descriptor, HTTP_STATUS_OK, NULL, NULL, NULL, 0,
			                                     CONNECTION_SEND_FLAGS_UN_MALLOCED);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
			}
		} else if(strcmp(httpRequest->head.requestLine.method, "OPTIONS") == 0) {
			HttpHeaderField* allowedHeader = (HttpHeaderField*)malloc(sizeof(HttpHeaderField));

			if(!allowedHeader) {
				LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
				return JobError_Malloc;
			}

			char* allowedHeaderBuffer = NULL;
			// all 405 have to have a Allow filed according to spec
			formatString(&allowedHeaderBuffer, return JobError_StringFormat;
			             , "%s%c%s", "Allow", '\0', "GET, POST, HEAD, OPTIONS");

			allowedHeader[0].key = allowedHeaderBuffer;
			allowedHeader[0].value = allowedHeaderBuffer + strlen(allowedHeaderBuffer) + 1;

			int result =
			    sendMessageToConnection(descriptor, HTTP_STATUS_OK, NULL, NULL, allowedHeader, 1,
			                            CONNECTION_SEND_FLAGS_UN_MALLOCED);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
			}
		} else {
			int result = sendMessageToConnection(descriptor, HTTP_STATUS_INTERNAL_SERVER_ERROR,
			                                     "Internal Server Error 1", MIME_TYPE_TEXT, NULL, 0,
			                                     CONNECTION_SEND_FLAGS_UN_MALLOCED);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
			}
		}
	} else if(isSupported == REQUEST_INVALID_HTTP_VERSION) {
		int result = sendMessageToConnection(descriptor, HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED,
		                                     "Only HTTP/1.1 is supported atm", MIME_TYPE_TEXT, NULL,
		                                     0, CONNECTION_SEND_FLAGS_UN_MALLOCED);

		if(result) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
		}
	} else if(isSupported == REQUEST_METHOD_NOT_SUPPORTED) {

		HttpHeaderField* allowedHeader = (HttpHeaderField*)malloc(sizeof(HttpHeaderField));

		if(!allowedHeader) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return JobError_Malloc;
		}

		char* allowedHeaderBuffer = NULL;
		// all 405 have to have a Allow filed according to spec
		formatString(&allowedHeaderBuffer, return JobError_StringFormat;
		             , "%s%c%s", "Allow", '\0', "GET, POST, HEAD, OPTIONS");

		allowedHeader[0].key = allowedHeaderBuffer;
		allowedHeader[0].value = allowedHeaderBuffer + strlen(allowedHeaderBuffer) + 1;

		int result = sendMessageToConnection(
		    descriptor, HTTP_STATUS_METHOD_NOT_ALLOWED,
		    "This primitive HTTP Server only supports GET, POST, HEAD and OPTIONS requests",
		    MIME_TYPE_TEXT, allowedHeader, 1, CONNECTION_SEND_FLAGS_UN_MALLOCED);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
		}
	} else if(isSupported == REQUEST_INVALID_NONEMPTY_BODY) {
		int result = sendMessageToConnection(
		    descriptor, HTTP_STATUS_BAD_REQUEST, "A GET, HEAD or OPTIONS Request can't have a body",
		    MIME_TYPE_TEXT, NULL, 0, CONNECTION_SEND_FLAGS_UN_MALLOCED);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
		}
	} else {
		int result = sendMessageToConnection(descriptor, HTTP_STATUS_INTERNAL_SERVER_ERROR,
		                                     "Internal Server Error 2", MIME_TYPE_TEXT, NULL, 0,
		                                     CONNECTION_SEND_FLAGS_UN_MALLOCED);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
		}
	}

	freeHttpRequest(httpRequest);

cleanup:
	// finally close the connection
	int result = close_connection_descriptor(descriptor, context);
	checkForError(result, "While trying to close the connection descriptor", return JobError_Close);
	// and free the malloced argument
	FREE_AT_END();
	return JobError_None;
}

#undef FREE_AT_END

// implemented specifically for the http Server, it just gets the internal value, but it's better to
// not access that, since additional steps can be required, like  boundary checks!
static int myqueue_size(myqueue* q) {
	if(q->size < 0) {
		fprintf(stderr,
		        "FATAL: internal size implementation error in the queue, value negative: %d!",
		        q->size);
	}
	return q->size;
}

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(ListenerError*) __listener_thread_function(anyType(ThreadArgument*) arg) {

	set_thread_name("listener thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting\n");

	ThreadArgument argument = *((ThreadArgument*)arg);

#define POLL_FD_AMOUNT 2

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[0].fd = argument.socketFd;
	poll_fds[0].events = POLLIN;

	sigset_t mySigset;
	sigemptyset(&mySigset);
	sigaddset(&mySigset, SIGINT);
	int sigFd = signalfd(-1, &mySigset, 0);
	checkForError(sigFd, "While trying to cancel the listener Thread on signal",
	              exit(EXIT_FAILURE););

	poll_fds[1].fd = sigFd;
	poll_fds[1].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO: Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socketFd or the signalFd
		int status = 0;
		while(status == 0) {
			status = poll(poll_fds, POLL_FD_AMOUNT, 5000);
			if(status < 0) {
				LOG_MESSAGE(LogLevelError, "poll failed: %s\n", strerror(errno));
				continue;
			}
		}

		if(poll_fds[1].revents == POLLIN || signal_received != 0) {
			// TODO: This fd isn't closed, when pthread_cancel is called from somewhere else,
			// fix that somehow
			close(poll_fds[1].fd);
			int result = pthread_cancel(pthread_self());
			checkForError(result, "While trying to cancel the listener Thread on signal",
			              return ListenerError_ThreadCancel;);
		}

		// the poll didn't see a POLLIN event in the argument.socketFd fd, so the accept
		// will fail, just redo the poll
		if(poll_fds[0].revents != POLLIN) {
			continue;
		}

		// would be better to set cancel state in the right places!!
		int connectionFd = accept(argument.socketFd, NULL, NULL);
		checkForError(connectionFd, "While Trying to accept a socket", continue;);

		ConnectionArgument* connectionArgument =
		    (ConnectionArgument*)malloc(sizeof(ConnectionArgument));

		if(!connectionArgument) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return ListenerError_Malloc;
		}

		// to have longer lifetime, that is needed here, since otherwise it would be "dead"
		connectionArgument->contexts = argument.contexts;
		connectionArgument->connectionFd = connectionFd;
		connectionArgument->listenerThread = pthread_self();
		connectionArgument->webSocketManager = argument.webSocketManager;

		// push to the queue, but not await, since when we wait it wouldn't be fast and
		// ready to accept new connections
		if(myqueue_push(argument.jobIds, pool_submit(argument.pool, __socket_connection_handler,
		                                             connectionArgument)) < 0) {
			return ListenerError_QueuePush;
		}

		// not waiting directly, but when the queue grows to fast, it is reduced, then the
		// listener thread MIGHT block, but probably are these first jobs already finished,
		// so its super fast,but if not doing that, the queue would overflow, nothing in
		// here is a cancellation point, so it's safe to cancel here, since only accept then
		// really cancels
		int size = myqueue_size(argument.jobIds);
		if(size > MAX_QUEUE_SIZE) {
			int boundary = size / 2;
			while(size > boundary) {

				job_id* jobId = (job_id*)myqueue_pop(argument.jobIds);

				JobError result = pool_await(jobId);

				if(is_job_error(result)) {
					if(result != JobError_None) {
						print_job_error(result);
					}
				} else if(result == PTHREAD_CANCELED) {
					LOG_MESSAGE_SIMPLE(LogLevelError, "A connection thread was cancelled!\n");
				} else {
					LOG_MESSAGE(LogLevelError,
					            "A connection thread was terminated with wrong error: %p!\n",
					            result);
				}

				--size;
			}
		}

		// gets cancelled in accept, there it also is the most time!
		// otherwise if it would cancel other functions it would be baaaad, but only accept
		// is here a cancel point!
	}
}

int startServer(uint16_t port, SecureOptions* const options) {

	// using TCP  and not 0, which is more explicit about what protocol to use
	// so essentially a socket is created, the protocol is AF_INET alias the IPv4 Prototol,
	// the socket type is SOCK_STREAM, meaning it has reliable read and write capabilities,
	// all other types are not that well suited for that example
	int socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	checkForError(socketFd, "While Trying to create socket", return EXIT_FAILURE;);

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int optionReturn = setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	checkForError(optionReturn, "While Trying to set socket option 'SO_REUSEPORT'",
	              return EXIT_FAILURE;);

	// creating the sockaddr_in struct, each number that is used in context of network has
	// to be converted into ntework byte order (Big Endian, linux uses Little Endian) that
	// is relevant for each multibyte value, essentially everything but char, so htox is
	// used, where x stands for different lengths of numbers, s for int, l for long
	struct sockaddr_in* addr =
	    (struct sockaddr_in*)mallocWithMemset(sizeof(struct sockaddr_in), true);

	if(!addr) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	addr->sin_family = AF_INET;
	// hto functions are used for networking, since there every number is BIG ENDIAN and
	// linux has Little Endian
	addr->sin_port = htons(port);
	// INADDR_ANY is 0.0.0.0, which means every port, but when nobody forwards it,
	// it means, that by default only localhost can be used to access it
	addr->sin_addr.s_addr = htonl(INADDR_ANY);

	// since bind is generic, the specific struct has to be casted, and the actual length
	// has to be given, this is a function signature, just to satisfy the typings, the real
	// requirements are given in the responsive protocol man page, here ip(7) also note that
	// ports below 1024 are  privileged ports, meaning, that you require special permissions
	// to be able to bind to them ( CAP_NET_BIND_SERVICE capability) (the simple way of
	// getting that is being root, or executing as root: sudo ...)
	int result = bind(socketFd, (struct sockaddr*)addr, sizeof(*addr));
	checkForError(result, "While trying to bind socket to port", return EXIT_FAILURE;);

	// SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listining on that socket, meaning
	// new connections can be accepted
	result = listen(socketFd, SOCKET_BACKLOG_SIZE);
	checkForError(result, "While trying to listen on socket", return EXIT_FAILURE;);

	const char* protocol_string = is_secure(options) ? "https" : "http";

	LOG_MESSAGE(LogLevelInfo, "To use this simple Http Server visit '%s://localhost:%d'.\n",
	            protocol_string, port);

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = receiveSignal;
	// initialize the mask to be empty
	int emptySetResult = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);
	int result1 = sigaction(SIGINT, &action, NULL);
	if(result1 < 0 || emptySetResult < 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't set signal interception: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	// create pool and queue! then initializing both!
	// the pool is created and destroyed outside of the listener, so the listener can be
	// cancelled and then the main thread destroys everything accordingly
	thread_pool pool;
	int create_result = pool_create_dynamic(&pool);
	if(create_result < 0) {
		print_create_error(-create_result);
		return EXIT_FAILURE;
	}

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	myqueue jobIds;
	if(myqueue_init(&jobIds) < 0) {
		return EXIT_FAILURE;
	};

	// this is an array of pointers
	ConnectionContext** contexts = malloc(sizeof(ConnectionContext*) * pool.workerThreadAmount);

	if(!contexts) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	for(size_t i = 0; i < pool.workerThreadAmount; ++i) {
		contexts[i] = get_connection_context(options);
	}

	WebSocketThreadManager* webSocketManager = initialize_thread_manager();

	// initializing the thread Arguments for the single listener thread, it receives all
	// necessary arguments
	pthread_t listenerThread;
	ThreadArgument threadArgument = { .pool = &pool,
		                              .jobIds = &jobIds,
		                              .contexts = contexts,
		                              .socketFd = socketFd,
		                              .webSocketManager = webSocketManager };

	// creating the thread
	result = pthread_create(&listenerThread, NULL, __listener_thread_function, &threadArgument);
	checkForThreadError(result, "An Error occurred while trying to create a new Thread",
	                    return EXIT_FAILURE;);

	// wait for the single listener thread to finish, that happens when he is cancelled via
	// shutdown request
	ListenerError returnValue;
	result = pthread_join(listenerThread, &returnValue);
	checkForThreadError(result, "An Error occurred while trying to wait for a Thread",
	                    return EXIT_FAILURE;);

	if(is_listener_error(returnValue)) {
		if(returnValue != ListenerError_None) {
			print_listener_error(returnValue);
		}
	} else if(returnValue != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "The listener thread wasn't cancelled properly!\n");
	} else if(returnValue == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError, "The listener thread was terminated with wrong error: %p!\n",
		            returnValue);
	}

	// since the listener doesn't wait on the jobs, the main thread has to do that work!
	// the queue can be filled, which can lead to a problem!!
	while(!myqueue_is_empty(&jobIds)) {
		job_id* jobId = (job_id*)myqueue_pop(&jobIds);

		JobError result = pool_await(jobId);

		if(is_job_error(result)) {
			if(result != JobError_None) {
				print_job_error(result);
			}
		} else if(result == PTHREAD_CANCELED) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "A connection thread was cancelled!\n");
		} else {
			LOG_MESSAGE(LogLevelError, "A connection thread was terminated with wrong error: %p!\n",
			            result);
		}
	}

	// then after all were awaited the pool is destroyed
	if(pool_destroy(&pool) < 0) {
		return EXIT_FAILURE;
	}

	// then the queue is destroyed
	if(myqueue_destroy(&jobIds) < 0) {
		return EXIT_FAILURE;
	}

	// finally closing the whole socket, so that the port is useable by other programs or by
	// this again, NOTES: ip(7) states :" A TCP local socket address that has been bound is
	// unavailable for  some time after closing, unless the SO_REUSEADDR flag has been set.
	// Care should be taken when using this flag as it makes TCP less reliable." So
	// essentially saying, also correctly closed sockets aren't available after a certain
	// time, even if closed correctly!
	result = close(socketFd);
	checkForError(result, "While trying to close the socket", return EXIT_FAILURE;);

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(addr);

	for(size_t i = 0; i < pool.workerThreadAmount; ++i) {
		free_connection_context(contexts[i]);
	}

	if(!thread_manager_remove_all_connections(webSocketManager)) {
		return EXIT_FAILURE;
	}

	if(!free_thread_manager(webSocketManager)) {
		return EXIT_FAILURE;
	}

	free(contexts);

	free_secure_options(options);

	return EXIT_SUCCESS;
}
