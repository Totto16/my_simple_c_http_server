#include <errno.h>
#include <signal.h>

#include "./send.h"
#include "./server.h"
#include "generic/helper.h"
#include "generic/read.h"
#include "generic/secure.h"
#include "generic/signal_fd.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/thread_pool.h"
#include "utils/utils.h"
#include "ws/handler.h"
#include "ws/thread_manager.h"
#include "ws/ws.h"

#ifdef _SIMPLE_SERVER_USE_OPENSSL
#include <openssl/crypto.h>
#endif

#define SUPPORT_KEEPALIVE false

// returns wether the protocol, method is supported, atm only GET and HTTP 1.1 are supported, if
// returned an enum state, the caller has to handle errors
RequestSupportStatus is_request_supported(HttpRequest* request) {
	if(request->head.request_line.protocol_version != HTTPProtocolVersion1Dot1) {
		return RequestInvalidHttpVersion;
	}

	if(request->head.request_line.method == HTTPRequestMethodInvalid) {
		return RequestMethodNotSupported;
	}

	if((request->head.request_line.method == HTTPRequestMethodGet ||
	    request->head.request_line.method == HTTPRequestMethodHead ||
	    request->head.request_line.method == HTTPRequestMethodOptions) &&
	   strlen(request->body) != 0) {
		LOG_MESSAGE(LogLevelDebug, "Non Empty body in GET / HEAD or OPTIONS: '%s'\n",
		            request->body);
		return RequestInvalidNonemptyBody;
	}

	return RequestSupported;
}

static volatile sig_atomic_t
    g_signal_received = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    0;

// only setting the volatile sig_atomic_t g_signal_received' in here
static void receive_signal(int signal_number) {
	g_signal_received = signal_number;
}

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

ANY_TYPE(JobError*)
http_socket_connection_handler(ANY_TYPE(HTTPConnectionArgument*) arg_ign, WorkerInfo worker_info) {

	// attention arg is malloced!
	HTTPConnectionArgument* argument = (HTTPConnectionArgument*)arg_ign;

	ConnectionContext* context = argument->contexts[worker_info.worker_index];
	char* thread_name_buffer = NULL;
	FORMAT_STRING(&thread_name_buffer, return JOB_ERROR_STRING_FORMAT;
	              , "connection handler %lu", worker_info.worker_index);
	set_thread_name(thread_name_buffer);

	const RouteManager* route_manager = argument->route_manager;

#define FREE_AT_END() \
	do { \
		unset_thread_name(); \
		free(thread_name_buffer); \
		free(argument); \
	} while(false)

	bool sig_result = setup_sigpipe_signal_handler();

	if(!sig_result) {
		FREE_AT_END();
		return NULL;
	}

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting Connection handler\n");

	ConnectionDescriptor* const descriptor =
	    get_connection_descriptor(context, argument->connection_fd);

	if(descriptor == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "get_connection_descriptor failed\n");

		FREE_AT_END();
		return JOB_ERROR_DESC;
	}

	char* raw_http_request = read_string_from_connection(descriptor);

	if(!raw_http_request) {
		HTTPResponseToSend to_send = {
			.status = HttpStatusBadRequest,
			.body = http_response_body_from_static_string(
			    "Request couldn't be read, a connection error occurred!"),
			.mime_type = MIME_TYPE_TEXT,
			.additional_headers = STBDS_ARRAY_EMPTY
		};

		int result = send_http_message_to_connection(
		    descriptor, to_send, (SendSettings){ .compression_to_use = CompressionTypeNone });

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
		}

		goto cleanup;
	}

	// raw_http_request gets freed in here
	HttpRequest* http_request = parse_http_request(raw_http_request);

	// To test this error codes you can use '-X POST' with curl or
	// '--http2' (doesn't work, since http can only be HTTP/1.1, https can be HTTP 2 or QUIC
	// alias HTTP 3)

	// http_request can be null, then it wasn't parse-able, according to parseHttpRequest, see
	// there for more information
	if(http_request == NULL) {
		HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
			                           .body = http_response_body_from_static_string(
			                               "Request couldn't be parsed, it was malformed!"),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = STBDS_ARRAY_EMPTY };

		int result = send_http_message_to_connection(
		    descriptor, to_send, (SendSettings){ .compression_to_use = CompressionTypeNone });

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
		}

		goto cleanup;
	}

	// if the request is supported then the "beautiful" website is sent, if the path is /shutdown
	// a shutdown is issued

	RequestSettings* request_settings = get_request_settings(http_request);

	SendSettings send_settings = get_send_settings(request_settings);
	free_request_settings(request_settings);
	request_settings = NULL;

	const RequestSupportStatus is_supported = is_request_supported(http_request);

	if(is_supported == RequestSupported) {
		SelectedRoute* selected_route =
		    route_manager_get_route_for_request(route_manager, http_request);

		if(selected_route == NULL) {

			int result = 0;

			switch(http_request->head.request_line.method) {
				case HTTPRequestMethodGet:
				case HTTPRequestMethodPost:
				case HTTPRequestMethodHead: {

					HTTPResponseToSend to_send = { .status = HttpStatusNotFound,
						                           .body = http_response_body_from_static_string(
						                               "File not Found"),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers = STBDS_ARRAY_EMPTY };

					result = send_http_message_to_connection_advanced(
					    descriptor, to_send, send_settings, http_request->head);
					break;
				}
				case HTTPRequestMethodOptions: {
					HttpHeaderFields additional_headers = STBDS_ARRAY_EMPTY;

					char* allowed_header_buffer = NULL;
					// all 405 have to have a Allow filed according to spec
					FORMAT_STRING(
					    &allowed_header_buffer,
					    {
						    stbds_arrfree(additional_headers);
						    FREE_AT_END();
						    return JOB_ERROR_STRING_FORMAT;
					    },
					    "%s%c%s", "Allow", '\0', "GET, POST, HEAD, OPTIONS");

					add_http_header_field_by_double_str(&additional_headers, allowed_header_buffer);

					HTTPResponseToSend to_send = { .status = HttpStatusOk,
						                           .body = http_response_body_empty(),
						                           .mime_type = NULL,
						                           .additional_headers = additional_headers };

					result = send_http_message_to_connection(descriptor, to_send, send_settings);

					break;
				}
				case HTTPRequestMethodInvalid:
				default: {
					HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
						                           .body = http_response_body_from_static_string(
						                               "Internal Server Error 1"),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers = STBDS_ARRAY_EMPTY };

					result = send_http_message_to_connection(descriptor, to_send, send_settings);
					break;
				}
			}

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
			}
		} else {

			HTTPSelectedRoute selected_route_data = get_selected_route_data(selected_route);

			HTTPRouteData route_data = selected_route_data.data;

			int result = 0;

			switch(route_data.type) {
				case HTTPRouteTypeSpecial: {

					switch(route_data.data.special.type) {
						case HTTPRouteSpecialDataTypeShutdown: {
							LOG_MESSAGE_SIMPLE(LogLevelInfo, "Shutdown requested!\n");

							HTTPResponseToSend to_send = {
								.status = HttpStatusOk,
								.body = http_response_body_from_static_string("Shutting Down"),
								.mime_type = MIME_TYPE_TEXT,
								.additional_headers = STBDS_ARRAY_EMPTY
							};

							result = send_http_message_to_connection_advanced(
							    descriptor, to_send, send_settings, http_request->head);

							// just cancel the listener thread, then no new connection are accepted
							// and the main thread cleans the pool and queue, all jobs are finished
							// so shutdown gracefully
							int cancel_result = pthread_cancel(argument->listener_thread);
							CHECK_FOR_ERROR(cancel_result,
							                "While trying to cancel the listener Thread", {
								                FREE_AT_END();
								                return JOB_ERROR_THREAD_CANCEL;
							                });

							break;
						}
						case HTTPRouteSpecialDataTypeWs: {

							int ws_request_successful =
							    handle_ws_handshake(http_request, descriptor, send_settings);

							WsConnectionArgs websocket_args = get_ws_args_from_http_request(
							    route_data.data.special.data.ws.fragmented,
							    selected_route_data.path);

							if(ws_request_successful >= 0) {
								// move the context so that we can use it in the long standing web
								// socket thread
								ConnectionContext* new_context = copy_connection_context(context);
								argument->contexts[worker_info.worker_index] = new_context;

								if(!thread_manager_add_connection(
								       argument->web_socket_manager, descriptor, context,
								       websocket_function, websocket_args)) {
									free_http_request(http_request);
									FREE_AT_END();

									return JOB_ERROR_CONNECTION_ADD;
								}

								// finally free everything necessary

								free_http_request(http_request);
								FREE_AT_END();

								return JOB_ERROR_NONE;
							}

							// the error was already sent, just close the descriptor and free the
							// http request, this is done at the end of this big if else statements
							break;
						}
						default: {
							// TODO(Totto): refactor all these arbitrary -<int> error numbers into
							// some error enum, e.g also -11
							result =
							    -10; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
							break;
						}
					}

					break;
				}

				case HTTPRouteTypeNormal: {

					result = route_manager_execute_route(
					    route_data.data.normal, descriptor, send_settings, http_request, context,
					    selected_route_data.path, selected_route_data.auth_user);

					break;
				}
				case HTTPRouteTypeInternal: {
					result = send_http_message_to_connection_advanced(
					    descriptor, route_data.data.internal.send, send_settings,
					    http_request->head);
					break;
				}
				default: {
					HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
						                           .body = http_response_body_from_static_string(
						                               "Internal error: Implementation error"),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers = STBDS_ARRAY_EMPTY };
					result = send_http_message_to_connection_advanced(
					    descriptor, to_send, send_settings, http_request->head);
					break;
				}
			}

			free_selected_route(selected_route);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
			}
		}
	} else if(is_supported == RequestInvalidHttpVersion) {
		HTTPResponseToSend to_send = { .status = HttpStatusHttpVersionNotSupported,
			                           .body = http_response_body_from_static_string(
			                               "Only HTTP/1.1 is supported atm"),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = STBDS_ARRAY_EMPTY };

		int result = send_http_message_to_connection_advanced(descriptor, to_send, send_settings,
		                                                      http_request->head);

		if(result) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
		}
	} else if(is_supported == RequestMethodNotSupported) {

		HttpHeaderFields additional_headers = STBDS_ARRAY_EMPTY;

		char* allowed_header_buffer = NULL;
		// all 405 have to have a Allow filed according to spec
		FORMAT_STRING(
		    &allowed_header_buffer,
		    {
			    stbds_arrfree(additional_headers);
			    FREE_AT_END();
			    return JOB_ERROR_STRING_FORMAT;
		    },
		    "%s%c%s", "Allow", '\0', "GET, POST, HEAD, OPTIONS");

		add_http_header_field_by_double_str(&additional_headers, allowed_header_buffer);

		HTTPResponseToSend to_send = {
			.status = HttpStatusMethodNotAllowed,
			.body = http_response_body_from_static_string(
			    "This primitive HTTP Server only supports GET, POST, HEAD and OPTIONS requests"),
			.mime_type = MIME_TYPE_TEXT,
			.additional_headers = additional_headers
		};

		int result = send_http_message_to_connection_advanced(descriptor, to_send, send_settings,
		                                                      http_request->head);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
		}
	} else if(is_supported == RequestInvalidNonemptyBody) {
		HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
			                           .body = http_response_body_from_static_string(
			                               "A GET, HEAD or OPTIONS Request can't have a body"),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = STBDS_ARRAY_EMPTY };

		int result = send_http_message_to_connection_advanced(descriptor, to_send, send_settings,
		                                                      http_request->head);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
		}
	} else {
		HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
			                           .body = http_response_body_from_static_string(
			                               "Internal Server Error 2"),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = STBDS_ARRAY_EMPTY };

		int result = send_http_message_to_connection_advanced(descriptor, to_send, send_settings,
		                                                      http_request->head);

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
		}
	}

	free_http_request(http_request);

cleanup:
	// finally close the connection
	int result = close_connection_descriptor_advanced(descriptor, context, SUPPORT_KEEPALIVE);
	CHECK_FOR_ERROR(result, "While trying to close the connection descriptor", {
		FREE_AT_END();
		return JOB_ERROR_CLOSE;
	});
	// and free the malloced argument
	FREE_AT_END();
	return JOB_ERROR_NONE;
}

#undef FREE_AT_END

// implemented specifically for the http Server, it just gets the internal value, but it's better to
// not access that, since additional steps can be required, like  boundary checks!
static int myqueue_size(Myqueue* queue) {
	if(queue->size < 0) {
		LOG_MESSAGE(LogLevelCritical,
		            "internal size implementation error in the queue, value negative: %d!",
		            queue->size);
	}
	return queue->size;
}

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
ANY_TYPE(ListenerError*) http_listener_thread_function(ANY_TYPE(HTTPThreadArgument*) arg) {

	set_thread_name("listener thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting\n");

	HTTPThreadArgument argument = *((HTTPThreadArgument*)arg);

	RUN_LIFECYCLE_FN(argument.fns.startup_fn);

#define POLL_FD_AMOUNT 2

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[0].fd = argument.socket_fd;
	poll_fds[0].events = POLLIN;

	int sig_fd = get_signal_like_fd(SIGINT);
	// TODO(Totto): don't exit here
	CHECK_FOR_ERROR(sig_fd, "While trying to cancel the listener Thread on signal",
	                exit(EXIT_FAILURE););

	poll_fds[1].fd = sig_fd;
	poll_fds[1].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO(Totto): Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socket_fd or the signalFd
		int status = 0;
		while(status == 0) {
			status = poll(
			    poll_fds, POLL_FD_AMOUNT,
			    5000); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			if(status < 0) {
				LOG_MESSAGE(LogLevelError, "poll failed: %s\n", strerror(errno));
				continue;
			}
		}

		if(poll_fds[1].revents == POLLIN || g_signal_received != 0) {
			// TODO(Totto): This fd isn't closed, when pthread_cancel is called from somewhere else,
			// fix that somehow
			close(poll_fds[1].fd);
			RUN_LIFECYCLE_FN(argument.fns.shutdown_fn);
			int result = pthread_cancel(pthread_self());
			CHECK_FOR_ERROR(result, "While trying to cancel the listener Thread on signal",
			                return LISTENER_ERROR_THREAD_CANCEL;);
			return LISTENER_ERROR_THREAD_AFTER_CANCEL;
		}

		// the poll didn't see a POLLIN event in the argument.socket_fd fd, so the accept
		// will fail, just redo the poll
		if(poll_fds[0].revents != POLLIN) {
			continue;
		}

		// would be better to set cancel state in the right places!!
		int connection_fd = accept(argument.socket_fd, NULL, NULL);
		CHECK_FOR_ERROR(connection_fd, "While Trying to accept a socket",
		                return LISTENER_ERROR_ACCEPT;);

		HTTPConnectionArgument* connection_argument =
		    (HTTPConnectionArgument*)malloc(sizeof(HTTPConnectionArgument));

		if(!connection_argument) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return LISTENER_ERROR_MALLOC;
		}

		// to have longer lifetime, that is needed here, since otherwise it would be "dead"
		connection_argument->contexts = argument.contexts;
		connection_argument->connection_fd = connection_fd;
		connection_argument->listener_thread = pthread_self();
		connection_argument->web_socket_manager = argument.web_socket_manager;
		connection_argument->route_manager = argument.route_manager;

		// push to the queue, but not await, since when we wait it wouldn't be fast and
		// ready to accept new connections
		if(myqueue_push(argument.job_ids, pool_submit(argument.pool, http_socket_connection_handler,
		                                              connection_argument)) < 0) {
			return LISTENER_ERROR_QUEUE_PUSH;
		}

		// not waiting directly, but when the queue grows to fast, it is reduced, then the
		// listener thread MIGHT block, but probably are these first jobs already finished,
		// so its super fast,but if not doing that, the queue would overflow, nothing in
		// here is a cancellation point, so it's safe to cancel here, since only accept then
		// really cancels
		int size = myqueue_size(argument.job_ids);
		if(size > HTTP_MAX_QUEUE_SIZE) {
			int boundary = size / 2;
			while(size > boundary) {

				JobId* job_id = (JobId*)myqueue_pop(argument.job_ids);

				JobError result = pool_await(job_id);

				if(is_job_error(result)) {
					if(result != JOB_ERROR_NONE) {
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

	RUN_LIFECYCLE_FN(argument.fns.shutdown_fn);
}

int start_http_server(uint16_t port, SecureOptions* const options,
                      AuthenticationProviders* const auth_providers) {

	// using TCP  and not 0, which is more explicit about what protocol to use
	// so essentially a socket is created, the protocol is AF_INET alias the IPv4 Prototol,
	// the socket type is SOCK_STREAM, meaning it has reliable read and write capabilities,
	// all other types are not that well suited for that example
	int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	CHECK_FOR_ERROR(socket_fd, "While Trying to create socket", return EXIT_FAILURE;);

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int option_return = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	CHECK_FOR_ERROR(option_return, "While Trying to set socket option 'SO_REUSEPORT'",
	                return EXIT_FAILURE;);

	// creating the sockaddr_in struct, each number that is used in context of network has
	// to be converted into ntework byte order (Big Endian, linux uses Little Endian) that
	// is relevant for each multibyte value, essentially everything but char, so htox is
	// used, where x stands for different lengths of numbers, s for int, l for long
	struct sockaddr_in* addr =
	    (struct sockaddr_in*)malloc_with_memset(sizeof(struct sockaddr_in), true);

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
	int result = bind(socket_fd, (struct sockaddr*)addr, sizeof(*addr));
	CHECK_FOR_ERROR(result, "While trying to bind socket to port", return EXIT_FAILURE;);

	// SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listening on that socket, meaning
	// new connections can be accepted
	result = listen(socket_fd, HTTP_SOCKET_BACKLOG_SIZE);
	CHECK_FOR_ERROR(result, "While trying to listen on socket", return EXIT_FAILURE;);

	const char* protocol_string =
	    is_secure(options) ? "https" : "http"; // NOLINT(readability-implicit-bool-conversion)

	LOG_MESSAGE(LogLevelInfo, "To use this simple Http Server visit '%s://localhost:%d'.\n",
	            protocol_string, port);

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = receive_signal;
	// initialize the mask to be empty
	int empty_set_result = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);
	int result1 = sigaction(SIGINT, &action, NULL);
	if(result1 < 0 || empty_set_result < 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't set signal interception: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	// create pool and queue! then initializing both!
	// the pool is created and destroyed outside of the listener, so the listener can be
	// cancelled and then the main thread destroys everything accordingly
	ThreadPool pool;
	int create_result = pool_create_dynamic(&pool);
	if(create_result < 0) {
		print_create_error(-create_result);
		return EXIT_FAILURE;
	}

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	Myqueue job_ids;
	if(myqueue_init(&job_ids) < 0) {
		return EXIT_FAILURE;
	};

	// this is an array of pointers
	STBDS_ARRAY(ConnectionContext*) contexts = STBDS_ARRAY_EMPTY;

	stbds_arrsetlen( // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
	    contexts, pool.worker_threads_amount);

	if(!contexts) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
		contexts[i] = get_connection_context(options);
	}

	WebSocketThreadManager* web_socket_manager = initialize_thread_manager();

	if(!web_socket_manager) {
		for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
			free_connection_context(contexts[i]);
		}
		stbds_arrfree(contexts);

		return EXIT_FAILURE;
	}

	HTTPRoutes default_routes = get_default_routes();

	RouteManager* route_manager = initialize_route_manager(default_routes, auth_providers);

	if(!route_manager) {
		for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
			free_connection_context(contexts[i]);
		}
		stbds_arrfree(contexts);

		if(!free_thread_manager(web_socket_manager)) {
			return EXIT_FAILURE;
		}

		return EXIT_FAILURE;
	}

	// initializing the thread Arguments for the single listener thread, it receives all
	// necessary arguments
	pthread_t listener_thread = {};
	HTTPThreadArgument thread_argument = { .pool = &pool,
		                                   .job_ids = &job_ids,
		                                   .contexts = contexts,
		                                   .socket_fd = socket_fd,
		                                   .web_socket_manager = web_socket_manager,
		                                   .route_manager = route_manager,
		                                   .fns = { .startup_fn = NULL, .shutdown_fn = NULL } };

	// creating the thread
	result =
	    pthread_create(&listener_thread, NULL, http_listener_thread_function, &thread_argument);
	CHECK_FOR_THREAD_ERROR(result, "An Error occurred while trying to create a new Thread",
	                       return EXIT_FAILURE;);

	// wait for the single listener thread to finish, that happens when he is cancelled via
	// shutdown request
	ListenerError return_value = LISTENER_ERROR_NONE;
	result = pthread_join(listener_thread, &return_value);
	CHECK_FOR_THREAD_ERROR(result, "An Error occurred while trying to wait for a Thread",
	                       return EXIT_FAILURE;);

	if(is_listener_error(return_value)) {
		if(return_value != LISTENER_ERROR_NONE) {
			print_listener_error(return_value);
		}
	} else if(return_value != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "The http listener thread wasn't cancelled properly!\n");
	} else if(return_value == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The http listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError,
		            "The http listener thread was terminated with wrong error: %p!\n",
		            return_value);
	}

	// since the listener doesn't wait on the jobs, the main thread has to do that work!
	// the queue can be filled, which can lead to a problem!!
	while(!myqueue_is_empty(&job_ids)) {
		JobId* job_id = (JobId*)myqueue_pop(&job_ids);

		JobError result = pool_await(job_id);

		if(is_job_error(result)) {
			if(result != JOB_ERROR_NONE) {
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
	if(myqueue_destroy(&job_ids) < 0) {
		return EXIT_FAILURE;
	}

	// finally closing the whole socket, so that the port is useable by other programs or by
	// this again, NOTES: ip(7) states :" A TCP local socket address that has been bound is
	// unavailable for  some time after closing, unless the SO_REUSEADDR flag has been set.
	// Care should be taken when using this flag as it makes TCP less reliable." So
	// essentially saying, also correctly closed sockets aren't available after a certain
	// time, even if closed correctly!
	result = close(socket_fd);
	CHECK_FOR_ERROR(result, "While trying to close the socket", return EXIT_FAILURE;);

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(addr);

	for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
		free_connection_context(contexts[i]);
	}

	if(!thread_manager_remove_all_connections(web_socket_manager)) {
		return EXIT_FAILURE;
	}

	if(!free_thread_manager(web_socket_manager)) {
		return EXIT_FAILURE;
	}

	free_route_manager(route_manager);

	stbds_arrfree(contexts);

	free_secure_options(options);

	free_authentication_providers(auth_providers);

	return EXIT_SUCCESS;
}
