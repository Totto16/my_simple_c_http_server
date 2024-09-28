/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include <signal.h>

#include "secure.h"
#include "send.h"
#include "server.h"
#include "thread_pool.h"
#include "utils.h"

// helper function that read string from connection, it handles everything that is necessary and
// returns an malloced (also realloced probably) pointer to a string, that is null terminated
char* readStringFromConnection(const ConnectionDescriptor* const descriptor) {
	// this buffer expands using realloc!!
	// also not the + 1 and the zero initialization, means that it's null terminated
	char* messageBuffer = (char*)mallocOrFail(INITIAL_MESSAGE_BUF_SIZE + 1, true);

	int buffersUsed = 0;
	while(true) {
		// read bytes, save the amount of read bytes, and then test for various scenarious
		int readBytes = read_from_descriptor(
		    descriptor, messageBuffer + (INITIAL_MESSAGE_BUF_SIZE * buffersUsed),
		    INITIAL_MESSAGE_BUF_SIZE);
		if(readBytes == -1) {
			// exit is a bit harsh, but atm there is no better error handling mechanism implemented,
			// that isn't necessary for that task
			perror("ERROR: Reading from a connection");
			exit(EXIT_FAILURE);
		} else if(readBytes == 0) {
			// client disconnected, so done
			break;
		} else if(readBytes == INITIAL_MESSAGE_BUF_SIZE) {
			// now the buffer has to be reused, so it's realloced, the used realloc helper also
			// initializes it with 0 and copies the old content, so nothing is lost and a new
			// INITIAL_MESSAGE_BUF_SIZE capacity is available + a null byte at the end
			size_t oldSize = ((buffersUsed + 1) * INITIAL_MESSAGE_BUF_SIZE) + 1;
			messageBuffer = (char*)reallocOrFail(messageBuffer, oldSize,
			                                     oldSize + INITIAL_MESSAGE_BUF_SIZE, true);
			++buffersUsed;
		} else {
			// the message was shorter and could fit in the existing buffer!
			break;
		}
	}

	// malloced, null terminated an probably "huge"
	return messageBuffer;
}

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
// pool, but the listenere adds it
// it receives all the necessary information and also handles the html pasring and response

JobResult connectionHandler(job_arg arg, WorkerInfo workerInfo) {

	// attention arg is malloced!
	ConnectionArgument argument = *((ConnectionArgument*)arg);

	ConnectionContext* context = argument.contexts[workerInfo.workerIndex];

	const ConnectionDescriptor* const descriptor =
	    get_connection_descriptor(context, argument.connectionFd);

	if(descriptor == NULL) {
		fprintf(stderr, "Error: get_connection_descriptor failed\n");

		JobError* error = mallocOrFail(sizeof(JobError), true);
		error->error_code = JobErrorCode_DESC;

		return error;
	}

	char* rawHttpRequest = readStringFromConnection(descriptor);
	// rawHttpRequest gets freed in here
	HttpRequest* httpRequest = parseHttpRequest(rawHttpRequest);

	// To test this error codes you can use '-X POST' with curl or
	// '--http2' (doesn't work, since http can only be HTTP/1.1, https can be HTTP 2 or QUIC
	// alias HTTP 3)

	// httpRequest can be null, then it wasn't parseable, according to parseHttpRequest, see
	// there for more information
	if(httpRequest == NULL) {
		sendMessageToConnection(descriptor, HTTP_STATUS_BAD_REQUEST,
		                        "Request couldn't be parsed, it was malformed!", MIME_TYPE_TEXT,
		                        NULL, 0, CONNECTION_SEND_FLAGS_UN_MALLOCED);
	} else {
		// if the request is supported then the "beautiful" website is sent, if the URI is /shutdown
		// a shutdown is issued

		const int isSupported = isRequestSupported(httpRequest);

		if(isSupported == REQUEST_SUPPORTED) {
			if(strcmp(httpRequest->head.requestLine.method, "GET") == 0) {
				// HTTP GET
				if(strcmp(httpRequest->head.requestLine.URI, "/shutdown") == 0) {
					printf("Shutdown requested!\n");
					sendMessageToConnection(descriptor, HTTP_STATUS_OK, "Shutting Down",
					                        MIME_TYPE_TEXT, NULL, 0,
					                        CONNECTION_SEND_FLAGS_UN_MALLOCED);
					// just cancel the listener thread, then no new connection are accepted and the
					// main thread cleans the pool and queue, all jobs are finished so shutdown
					// gracefully
					int result = pthread_cancel(argument.listenerThread);
					checkResultForErrorAndExit("While trying to cancel the listener Thread");

				} else if(strcmp(httpRequest->head.requestLine.URI, "/") == 0) {
					sendMessageToConnection(
					    descriptor, HTTP_STATUS_OK,
					    httpRequestToHtml(httpRequest, is_secure_context(context)), MIME_TYPE_HTML,
					    NULL, 0, CONNECTION_SEND_FLAGS_MALLOCED);
				} else {
					sendMessageToConnection(descriptor, HTTP_STATUS_NOT_FOUND, "", MIME_TYPE_TEXT,
					                        NULL, 0, CONNECTION_SEND_FLAGS_UN_MALLOCED);
				}
			} else if(strcmp(httpRequest->head.requestLine.method, "POST") == 0) {
				// HTTP POST

				sendMessageToConnection(descriptor, HTTP_STATUS_OK,
				                        httpRequestToJSON(httpRequest, is_secure_context(context)),
				                        MIME_TYPE_JSON, NULL, 0, CONNECTION_SEND_FLAGS_MALLOCED);
			} else if(strcmp(httpRequest->head.requestLine.method, "HEAD") == 0) {
				// TODO send actual Content-Length, experiment with e.g a large video file!
				sendMessageToConnection(descriptor, HTTP_STATUS_OK, "", MIME_TYPE_HTML, NULL, 0,
				                        CONNECTION_SEND_FLAGS_UN_MALLOCED);
			} else if(strcmp(httpRequest->head.requestLine.method, "OPTIONS") == 0) {
				HttpHeaderField* allowedHeader =
				    (HttpHeaderField*)mallocOrFail(sizeof(HttpHeaderField), true);

				char* allowedHeaderBuffer = NULL;
				// all 405 have to have a Allow filed according to spec
				formatString(&allowedHeaderBuffer, "%s%c%s", "Allow", '\0',
				             "GET, POST, HEAD, OPTIONS");

				allowedHeader[0].key = allowedHeaderBuffer;
				allowedHeader[0].value = allowedHeaderBuffer + strlen(allowedHeaderBuffer) + 1;

				sendMessageToConnection(descriptor, HTTP_STATUS_OK, "", MIME_TYPE_TEXT,
				                        allowedHeader, 1, CONNECTION_SEND_FLAGS_UN_MALLOCED);
			} else {
				sendMessageToConnection(descriptor, HTTP_STATUS_INTERNAL_SERVER_ERROR,
				                        "Internal Server Error 1", MIME_TYPE_TEXT, NULL, 0,
				                        CONNECTION_SEND_FLAGS_UN_MALLOCED);
			}
		} else if(isSupported == REQUEST_INVALID_HTTP_VERSION) {
			sendMessageToConnection(descriptor, HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED,
			                        "Only HTTP/1.1 is supported atm", MIME_TYPE_TEXT, NULL, 0,
			                        CONNECTION_SEND_FLAGS_UN_MALLOCED);
		} else if(isSupported == REQUEST_METHOD_NOT_SUPPORTED) {

			HttpHeaderField* allowedHeader =
			    (HttpHeaderField*)mallocOrFail(sizeof(HttpHeaderField), true);

			char* allowedHeaderBuffer = NULL;
			// all 405 have to have a Allow filed according to spec
			formatString(&allowedHeaderBuffer, "%s%c%s", "Allow", '\0', "GET, POST, HEAD, OPTIONS");

			allowedHeader[0].key = allowedHeaderBuffer;
			allowedHeader[0].value = allowedHeaderBuffer + strlen(allowedHeaderBuffer) + 1;

			sendMessageToConnection(
			    descriptor, HTTP_STATUS_METHOD_NOT_ALLOWED,
			    "This primitive HTTP Server only supports GET, POST, HEAD and OPTIONS requests",
			    MIME_TYPE_TEXT, allowedHeader, 1, CONNECTION_SEND_FLAGS_UN_MALLOCED);
		} else if(isSupported == REQUEST_INVALID_NONEMPTY_BODY) {
			sendMessageToConnection(descriptor, HTTP_STATUS_BAD_REQUEST,
			                        "A GET, HEAD or OPTIONS Request can't have a body",
			                        MIME_TYPE_TEXT, NULL, 0, CONNECTION_SEND_FLAGS_UN_MALLOCED);
		} else {
			sendMessageToConnection(descriptor, HTTP_STATUS_INTERNAL_SERVER_ERROR,
			                        "Internal Server Error 2", MIME_TYPE_TEXT, NULL, 0,
			                        CONNECTION_SEND_FLAGS_UN_MALLOCED);
		}

		freeHttpRequest(httpRequest);
	}

	// finally close the connection
	int result = close_connection_descriptor(descriptor, context);
	checkResultForErrorAndExit("While trying to close the connection descriptor");
	// and free the malloced argument
	free(arg);
	return NULL;
}

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
anyType(NULL) threadFunction(anyType(ThreadArgument*) arg) {

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
				perror("ERROR: Reading in poll");
				continue;
			}
		}

		if(poll_fds[1].revents == POLLIN || signal_received != 0) {
			// TODO: This fd isn't closed, when pthread_cancel is called from somewhere else,
			// fix that somehow
			close(poll_fds[1].fd);
			int result = pthread_cancel(pthread_self());
			checkResultForErrorAndExit("While trying to cancel the listener Thread on signal");
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
		    (ConnectionArgument*)mallocOrFail(sizeof(ConnectionArgument), true);
		// to have longer lifetime, that is needed here, since otherwise it would be "dead"
		connectionArgument->contexts = argument.contexts;
		connectionArgument->connectionFd = connectionFd;
		connectionArgument->listenerThread = pthread_self();

		// push to the queue, but not await, since when we wait it wouldn't be fast and
		// ready to accept new connections
		myqueue_push(argument.jobIds,
		             pool_submit(argument.pool, connectionHandler, connectionArgument));

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

				JobError* result = pool_await(jobId);

				if(result != NULL) {
					print_job_error(stderr, result);
					free_job_error(result);
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
	checkForError(socketFd, "While Trying to create socket", exit(EXIT_FAILURE););

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int optionReturn = setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	checkForError(optionReturn, "While Trying to set socket option 'SO_REUSEPORT'",
	              exit(EXIT_FAILURE););

	// creating the sockaddr_in struct, each number that is used in context of network has
	// to be converted into ntework byte order (Big Endian, linux uses Little Endian) that
	// is relevant for each multibyte value, essentially everything but char, so htox is
	// used, where x stands for different lengths of numbers, s for int, l for long
	struct sockaddr_in* addr = (struct sockaddr_in*)mallocOrFail(sizeof(struct sockaddr_in), true);
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
	checkResultForErrorAndExit("While trying to bind socket to port");

	// SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listining on that socket, meaning
	// new connections can be accepted
	result = listen(socketFd, SOCKET_BACKLOG_SIZE);
	checkResultForErrorAndExit("While trying to listen on socket");

	printf("To use this simple Http Server visit '%s://localhost:%d'.\n",
	       is_secure(options) ? "https" : "http", port);

	// set up the signal handler
	// just create a sitgaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = receiveSignal;
	// initilaize the mask to be empty
	int emptySetResult = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);
	int result1 = sigaction(SIGINT, &action, NULL);
	if(result1 < 0 || emptySetResult < 0) {
		perror("Couldn't set this signal");
		exit(EXIT_FAILURE);
	}

	// create pool and queue! then initializing both!
	// the pool is created and destroyed outside of the listener, so the listener can be
	// cancelled and then the main thread destroys everything accordingly
	thread_pool pool;
	pool_create_dynamic(&pool);

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	myqueue jobIds;
	myqueue_init(&jobIds);

	// this is an array of pointers
	ConnectionContext** contexts =
	    mallocOrFail(sizeof(ConnectionContext*) * pool.workerThreadAmount, true);

	for(size_t i = 0; i < pool.workerThreadAmount; ++i) {
		contexts[i] = get_connection_context(options);
	}

	// initializing the thread Arguments for the single listener thread, it receives all
	// necessary arguments
	pthread_t listenerThread;
	ThreadArgument threadArgument = {
		.pool = &pool, .jobIds = &jobIds, .contexts = contexts, .socketFd = socketFd
	};

	// creating the thread
	result = pthread_create(&listenerThread, NULL, threadFunction, &threadArgument);
	checkResultForThreadErrorAndExit("An Error occurred while trying to create a new Thread");

	// wait for the single listener thread to finish, that happens when he is cancelled via
	// shutdown request
	void* returnValue;
	result = pthread_join(listenerThread, &returnValue);
	checkResultForThreadErrorAndExit("An Error occurred while trying to wait for a Thread");
	if(returnValue != PTHREAD_CANCELED) {
		fprintf(stderr, "WARNING: the thread wasn't cancelled properly!");
	}

	// since the listener doesn't wait on the jobs, the main thread has to do that work!
	// the queue can be filled, which can lead to a problem!!
	while(!myqueue_is_empty(&jobIds)) {
		job_id* jobId = (job_id*)myqueue_pop(&jobIds);

		JobError* result = pool_await(jobId);

		if(result != NULL) {
			print_job_error(stderr, result);
			free_job_error(result);
		}
	}

	// then after all were awaited the pool is destroyed
	pool_destroy(&pool);
	// then the queue is destroyed
	myqueue_destroy(&jobIds);

	// finally closing the whole socket, so that the port is useable by other programs or by
	// this again, NOTES: ip(7) states :" A TCP local socket address that has been bound is
	// unavailable for  some time after closing, unless the SO_REUSEADDR flag has been set.
	// Care should be taken when using this flag as it makes TCP less reliable." So
	// essentially saying, also correctly closed sockets aren't available after a certain
	// time, even if closed correctly!
	result = close(socketFd);
	checkResultForErrorAndExit("While trying to close the socket");

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(addr);

	for(size_t i = 0; i < pool.workerThreadAmount; ++i) {
		free_connection_context(contexts[i]);
	}

	free(contexts);

	free_secure_options(options);

	return EXIT_SUCCESS;
}

void print_job_error(FILE* file, const JobError* const error) {

	const char* error_str = "Unknown error";

	switch(error->error_code) {
		case JobErrorCode_DESC: error_str = "Description"; break;
		default: break;
	}

	fprintf(file, "Job Error: %s\n", error_str);
}

void free_job_error(JobError* error) {
	free(error);
}
