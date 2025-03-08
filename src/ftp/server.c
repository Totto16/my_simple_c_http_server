

#include "./server.h"
#include "./command.h"
#include "./protocol.h"
#include "./send.h"
#include "./state.h"

#include "generic/helper.h"
#include "generic/read.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/thread_pool.h"
#include "utils/utils.h"

#include <errno.h>
#include <netinet/ip.h>
#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#define ANON_USERNAME "anonymous"

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

anyType(JobError*)
    ftp_control_socket_connection_handler(anyType(FTPControlConnectionArgument*) _arg,
                                          WorkerInfo workerInfo) {

	// attention arg is malloced!
	FTPControlConnectionArgument* argument = (FTPControlConnectionArgument*)_arg;

	ConnectionContext* context = argument->contexts[workerInfo.workerIndex];
	char* thread_name_buffer = NULL;
	formatString(&thread_name_buffer, return JobError_StringFormat;
	             , "connection handler %lu", workerInfo.workerIndex);
	set_thread_name(thread_name_buffer);

	bool signal_result = setup_sigpipe_signal_handler();

	if(!signal_result) {
		return JobError_SigHandler;
	}

	struct sockaddr_in server_addr;
	socklen_t addr_len = sizeof(server_addr);

	// would be better to set cancel state in the right places!!
	int socknameResult =
	    getsockname(argument->connectionFd, (struct sockaddr*)&server_addr, &addr_len);
	if(socknameResult != 0) {
		LOG_MESSAGE(LogLevelError | LogPrintLocation, "getsockname error: %s\n", strerror(errno));
		return JobError_GetSockName;
	}

	if(addr_len != sizeof(server_addr)) {
		LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "getsockname has wrong addr_len\n");
		return JobError_GetSockName;
	}

	struct sockaddr_in data_addr = {};
	memcpy(&data_addr, &server_addr, sizeof(server_addr));

	data_addr.sin_port = htons(argument->ports.data);

#define FREE_AT_END() \
	do { \
		free(thread_name_buffer); \
		free(argument); \
	} while(false)

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting Connection handler\n");

	ConnectionDescriptor* const descriptor =
	    get_connection_descriptor(context, argument->connectionFd);

	if(descriptor == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "get_connection_descriptor failed\n");

		FREE_AT_END();
		return JobError_Desc;
	}

	int hello_result =
	    sendFTPMessageToConnection(descriptor, FTP_RETURN_CODE_SRVC_READY, "Simple FTP Server",
	                               CONNECTION_SEND_FLAGS_UN_MALLOCED);
	if(hello_result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending hello message\n");
		goto cleanup;
	}

	bool quit = false;

	while(!quit) {

		char* rawFtpCommands = readStringFromConnection(descriptor);

		if(!rawFtpCommands) {
			int result =
			    sendFTPMessageToConnection(descriptor, FTP_RETURN_CODE_SYNTAX_ERROR,
			                               "Request couldn't be read, a connection error occurred!",
			                               CONNECTION_SEND_FLAGS_UN_MALLOCED);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
				goto cleanup;
			}

			continue;
		}

		// rawFtpCommands gets freed in here
		FTPCommandArray* ftpCommands = parseMultipleFTPCommands(rawFtpCommands);

		// ftpCommands can be null, then it wasn't parse-able, according to parseMultipleCommands,
		// see there for more information
		if(ftpCommands == NULL) {
			int result = sendFTPMessageToConnection(descriptor, FTP_RETURN_CODE_SYNTAX_ERROR,
			                                        "Invalid Command Sequence",
			                                        CONNECTION_SEND_FLAGS_UN_MALLOCED);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n");
				goto cleanup;
			}

			continue;
		}

		for(size_t i = 0; i < ftpCommands->size; ++i) {
			FTPCommand* command = ftpCommands->data[i];
			bool successfull = ftp_process_command(descriptor, data_addr, argument->state, command);
			if(!successfull) {
				quit = true;
				freeFTPCommandArray(ftpCommands);
				break;
			}
		}

		freeFTPCommandArray(ftpCommands);
	}

cleanup:
	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Closing Connection\n");
	// finally close the connection
	int result = close_connection_descriptor(descriptor, context);
	checkForError(result, "While trying to close the connection descriptor", {
		FREE_AT_END();
		return JobError_Close;
	});
	// and free the malloced argument
	FREE_AT_END();
	return JobError_None;
}

#undef FREE_AT_END

#define SEND_RESPONSE_WITH_ERROR_CHECK(code, msg) \
	do { \
		int result = \
		    sendFTPMessageToConnection(descriptor, code, msg, CONNECTION_SEND_FLAGS_UN_MALLOCED); \
		if(result < 0) { \
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n"); \
			return false; \
		} \
	} while(false)

#define SEND_RESPONSE_WITH_ERROR_CHECK_F(code, format, ...) \
	do { \
		StringBuilder* sb = string_builder_init(); \
		string_builder_append(sb, return false;, format, __VA_ARGS__); \
		int result = sendFTPMessageToConnectionSb(descriptor, code, sb); \
		if(result < 0) { \
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending response\n"); \
			return false; \
		} \
	} while(false)

bool ftp_process_command(ConnectionDescriptor* const descriptor, FTPConnectAddr data_addr,
                         FTPState* state, const FTPCommand* command) {

	switch(command->type) {
		case FTP_COMMAND_USER: {

			// see https://datatracker.ietf.org/doc/html/rfc1635
			if(strcasecmp(ANON_USERNAME, command->data.string) == 0) {
				free_account_data(state->account);

				state->account->state = ACCOUNT_STATE_OK;

				char* malloced_username = copy_cstr(command->data.string);

				if(!malloced_username) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR, "Internal ERROR!");

					return true;
				}

				AccountOkData ok_data = { .permissions = ACCOUNT_PERMISSIONS_READ,
					                      .username = malloced_username };

				state->account->data.ok_data = ok_data;

				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_USER_LOGGED_IN,
				                               "Logged In as anon!");

				return true;
			}

			free_account_data(state->account);

			state->account->state = ACCOUNT_STATE_ONLY_USER;

			char* malloced_username = copy_cstr(command->data.string);

			if(!malloced_username) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR, "Internal ERROR!");

				return true;
			}

			state->account->data.temp_data.username = malloced_username;

			SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_NEED_PSWD, "Need Password!");

			return true;
		}

		case FTP_COMMAND_PASS: {

			if(state->account->state == ACCOUNT_STATE_OK &&
			   strcasecmp(ANON_USERNAME, state->account->data.ok_data.username) == 0) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_USER_LOGGED_IN,
				                               "Already logged in as anon!");

				return true;
			}

			// TODO: allow user changing
			if(state->account->state != ACCOUNT_STATE_ONLY_USER) {
				free_account_data(state->account);

				state->account->state = ACCOUNT_STATE_EMPTY;

				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_BAD_SEQUENCE, "No user specified!");

				return true;
			}

			char* username = state->account->data.temp_data.username;

			char* passwd = command->data.string;

			USER_VALIDITY user_validity = account_verify(username, passwd);

			switch(user_validity) {
				case USER_VALIDITY_OK: {

					free_account_data(state->account);

					state->account->state = ACCOUNT_STATE_OK;

					char* malloced_username = copy_cstr(username);

					if(!malloced_username) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR,
						                               "Internal ERROR!");

						return true;
					}

					AccountOkData ok_data = { .permissions = ACCOUNT_PERMISSIONS_READ |
						                                     ACCOUNT_PERMISSIONS_WRITE,
						                      .username = malloced_username };

					state->account->data.ok_data = ok_data;

					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_USER_LOGGED_IN,
					                               "Logged In as user!");

					return true;
				}
				case USER_VALIDITY_NO_SUCH_USER: {

					free_account_data(state->account);

					state->account->state = ACCOUNT_STATE_EMPTY;

					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_NOT_LOGGED_IN,
					                               "No such user found!");

					return true;
				}
				case USER_VALIDITY_WRONG_PASSWORD: {
					free_account_data(state->account);

					state->account->state = ACCOUNT_STATE_EMPTY;

					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_NOT_LOGGED_IN,
					                               "Wrong password!");

					return true;
				}
				case USER_VALIDITY_INTERNAL_ERROR:
				default: {
					free_account_data(state->account);

					state->account->state = ACCOUNT_STATE_EMPTY;

					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_NOT_LOGGED_IN,
					                               "Internal Error!");

					return true;
				}
			}

			UNREACHABLE();
			return true;
		}

		case FTP_COMMAND_PWD: {

			if(state->account->state != ACCOUNT_STATE_OK) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_NOT_LOGGED_IN,
				                               "Not logged in: can't access files!");

				return true;
			}

			char* dirname = get_current_dir_name(state, true);

			if(!dirname) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR, "Internal Error!");

				return true;
			}

			// permission model: everybody that is logged in can use PWD

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FTP_RETURN_CODE_DIR_OP_SUCC, "\"%s\"", dirname);

			free(dirname);

			return true;
		}

		case FTP_COMMAND_PASV: {

			char* port_desc = make_address_port_desc(data_addr);

			if(!port_desc) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR, "Internal Error!");

				return true;
			}

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FTP_RETURN_CODE_ENTERING_PASSIVE_MODE,
			                                 "Entering Passive Mode %s.", port_desc);

			free(port_desc);

			return true;
		}

		case FTP_COMMAND_TYPE: {

			FTPCommandTypeInformation* type_info = command->data.type_info;
			if(!type_info->is_normal) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED_FOR_PARAM,
				                               "Not Implemented!");
				return true;
			}

			switch(type_info->data.type & FTP_TRANSMISSION_TYPE_MASK_BASE) {
				case FTP_TRANSMISSION_TYPE_ASCII: {
					state->current_type = FTP_TRANSMISSION_TYPE_ASCII;
					break;
				}
				case FTP_TRANSMISSION_TYPE_EBCDIC: {
					state->current_type = FTP_TRANSMISSION_TYPE_EBCDIC;
					break;
				}
				case FTP_TRANSMISSION_TYPE_IMAGE: {
					state->current_type = FTP_TRANSMISSION_TYPE_IMAGE;
					break;
				}
				default:
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR, "Internal ERROR!");

					return true;
			}

			// TODO also handle flags
			SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_CMD_OK, "Set Type!");

			return true;
		}

		case FTP_COMMAND_AUTH: {
#ifdef _SIMPLE_SERVER_SECURE_DISABLED
			SEND_RESPONSE_WITH_ERROR_CHECK(
			    FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED,
			    "AUTH not supported, server not build with ssl / tls enabled!");
			return true;
#else

			SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED,
			                               "AUTH recognized, but command not implemented!");

			return true;

#endif
		}

		default: {
			SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED,
			                               "Command not implemented!");

			return true;
		}
	}
}

// implemented specifically for the ftp Server, it just gets the internal value, but it's better
// to not access that, since additional steps can be required, like  boundary checks!
static int myqueue_size(myqueue* queue) {
	if(queue->size < 0) {
		fprintf(stderr,
		        "FATAL: internal size implementation error in the queue, value negative: %d!",
		        queue->size);
	}
	return queue->size;
}

static volatile sig_atomic_t
    signal_received = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    0;

// only setting the volatile sig_atomic_t signal_received' in here
static void receiveSignal(int signalNumber) {
	signal_received = signalNumber;
}

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(ListenerError*)
    ftp_control_listener_thread_function(anyType(FTPControlThreadArgument*) arg) {

	set_thread_name("listener thread (control)");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting thread\n");

	FTPControlThreadArgument argument = *((FTPControlThreadArgument*)arg);

#define POLL_FD_AMOUNT 2

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[0].fd = argument.socketFd;
	poll_fds[0].events = POLLIN;

	sigset_t mySigset;
	sigemptyset(&mySigset);
	sigaddset(&mySigset, SIGINT);
	int sigFd = signalfd(-1, &mySigset, 0);
	// TODO(Totto): don't exit here
	checkForError(sigFd, "While trying to cancel the listener Thread on signal",
	              exit(EXIT_FAILURE););

	poll_fds[1].fd = sigFd;
	poll_fds[1].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO(Totto): Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socketFd or the signalFd
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

		if(poll_fds[1].revents == POLLIN || signal_received != 0) {
			// TODO(Totto): This fd isn't closed, when pthread_cancel is called from somewhere
			// else, fix that somehow
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

		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		// would be better to set cancel state in the right places!!
		int connectionFd = accept(argument.socketFd, (struct sockaddr*)&client_addr, &addr_len);
		checkForError(connectionFd, "While Trying to accept a socket",
		              return ListenerError_Accept;);

		if(addr_len != sizeof(client_addr)) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Accept has wrong addr_len\n");
			return ListenerError_Accept;
		}

		FTPControlConnectionArgument* connectionArgument =
		    (FTPControlConnectionArgument*)malloc(sizeof(FTPControlConnectionArgument));

		if(!connectionArgument) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return ListenerError_Malloc;
		}

		FTPState* connection_ftp_state = alloc_default_state(argument.global_folder, client_addr);

		if(!connection_ftp_state) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return ListenerError_Malloc;
		}

		// to have longer lifetime, that is needed here, since otherwise it would be "dead"
		connectionArgument->contexts = argument.contexts;
		connectionArgument->connectionFd = connectionFd;
		connectionArgument->listenerThread = pthread_self();
		connectionArgument->state = connection_ftp_state;
		connectionArgument->addr = client_addr;
		connectionArgument->ports = argument.ports;

		// push to the queue, but not await, since when we wait it wouldn't be fast and
		// ready to accept new connections
		if(myqueue_push(argument.jobIds,
		                pool_submit(argument.pool, ftp_control_socket_connection_handler,
		                            connectionArgument)) < 0) {
			return ListenerError_QueuePush;
		}

		// not waiting directly, but when the queue grows to fast, it is reduced, then the
		// listener thread MIGHT block, but probably are these first jobs already finished,
		// so its super fast,but if not doing that, the queue would overflow, nothing in
		// here is a cancellation point, so it's safe to cancel here, since only accept then
		// really cancels
		int size = myqueue_size(argument.jobIds);
		if(size > FTP_MAX_QUEUE_SIZE) {
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

anyType(ListenerError*) ftp_data_listener_thread_function(anyType(FTPDataThreadArgument*) arg) {

	set_thread_name("listener thread (data)");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting thread\n");

	FTPDataThreadArgument argument = *((FTPDataThreadArgument*)arg);

#define POLL_FD_AMOUNT 2

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[0].fd = argument.socketFd;
	poll_fds[0].events = POLLIN;

	sigset_t mySigset;
	sigemptyset(&mySigset);
	sigaddset(&mySigset, SIGINT);
	int sigFd = signalfd(-1, &mySigset, 0);
	// TODO(Totto): don't exit here
	checkForError(sigFd, "While trying to cancel the listener Thread on signal",
	              exit(EXIT_FAILURE););

	poll_fds[1].fd = sigFd;
	poll_fds[1].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO(Totto): Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socketFd or the signalFd
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

		if(poll_fds[1].revents == POLLIN || signal_received != 0) {
			// TODO(Totto): This fd isn't closed, when pthread_cancel is called from somewhere
			// else, fix that somehow
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

		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		// would be better to set cancel state in the right places!!
		int connectionFd = accept(argument.socketFd, (struct sockaddr*)&client_addr, &addr_len);
		checkForError(connectionFd, "While Trying to accept a socket",
		              return ListenerError_Accept;);

		if(addr_len != sizeof(client_addr)) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Accept has wrong addr_len\n");
			return ListenerError_Accept;
		}

		LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Got a new data connection\n");

		// search this exact client address in the control connections, that wait for such things,
		// that is a array with entries we can search!
		// TODO
		/* 	if(is_in_that) {
		        // DO SOMETHING
		    }
	 */

		// TODO: remove
		close(connectionFd);
	}
}

int startFtpServer(PortSize control_port, PortSize data_port, char* folder) {

	// using TCP  and not 0, which is more explicit about what protocol to use
	// so essentially a socket is created, the protocol is AF_INET alias the IPv4 Prototol,
	// the socket type is SOCK_STREAM, meaning it has reliable read and write capabilities,
	// all other types are not that well suited for that example
	int controlSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	checkForError(controlSocketFd, "While Trying to create control socket", return EXIT_FAILURE;);

	int dataSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	checkForError(dataSocketFd, "While Trying to create data socket", return EXIT_FAILURE;);

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int optionReturn1 =
	    setsockopt(controlSocketFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	checkForError(optionReturn1, "While Trying to set the control socket option 'SO_REUSEPORT'",
	              return EXIT_FAILURE;);
	int optionReturn2 = setsockopt(dataSocketFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	checkForError(optionReturn2, "While Trying to set the data socket option 'SO_REUSEPORT'",
	              return EXIT_FAILURE;);

	// creating the sockaddr_in struct, each number that is used in context of network has
	// to be converted into ntework byte order (Big Endian, linux uses Little Endian) that
	// is relevant for each multibyte value, essentially everything but char, so htox is
	// used, where x stands for different lengths of numbers, s for int, l for long
	struct sockaddr_in* control_addr =
	    (struct sockaddr_in*)mallocWithMemset(sizeof(struct sockaddr_in), true);

	if(!control_addr) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in* data_addr =
	    (struct sockaddr_in*)mallocWithMemset(sizeof(struct sockaddr_in), true);

	if(!data_addr) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	control_addr->sin_family = AF_INET;
	data_addr->sin_family = AF_INET;
	// hto functions are used for networking, since there every number is BIG ENDIAN and
	// linux has Little Endian
	control_addr->sin_port = htons(control_port);
	data_addr->sin_port = htons(data_port);
	// INADDR_ANY is 0.0.0.0, which means every port, but when nobody forwards it,
	// it means, that by default only localhost can be used to access it
	control_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	data_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	// since bind is generic, the specific struct has to be casted, and the actual length
	// has to be given, this is a function signature, just to satisfy the typings, the real
	// requirements are given in the responsive protocol man page, here ip(7) also note that
	// ports below 1024 are  privileged ports, meaning, that you require special permissions
	// to be able to bind to them ( CAP_NET_BIND_SERVICE capability) (the simple way of
	// getting that is being root, or executing as root: sudo ...)
	int result1 = bind(controlSocketFd, (struct sockaddr*)control_addr, sizeof(*control_addr));
	checkForError(result1, "While trying to bind control socket to port", return EXIT_FAILURE;);
	int result2 = bind(dataSocketFd, (struct sockaddr*)data_addr, sizeof(*data_addr));
	checkForError(result2, "While trying to bind data socket to port", return EXIT_FAILURE;);

	// FTP_SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listening on that socket, meaning
	// new connections can be accepted
	result1 = listen(controlSocketFd, FTP_SOCKET_BACKLOG_SIZE);
	checkForError(result1, "While trying to listen on control socket", return EXIT_FAILURE;);

	result2 = listen(dataSocketFd, FTP_SOCKET_BACKLOG_SIZE);
	checkForError(result2, "While trying to listen on data socket", return EXIT_FAILURE;);

	LOG_MESSAGE(LogLevelInfo, "To use this simple FTP Server visit ftp://localhost:%d'.\n",
	            control_port);

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = receiveSignal;
	// initialize the mask to be empty
	int emptySetResult = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);
	int result_act = sigaction(SIGINT, &action, NULL);
	if(result_act < 0 || emptySetResult < 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't set signal interception: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	// create pool and queue! then initializing both!
	// the pool is created and destroyed outside of the listener, so the listener can be
	// cancelled and then the main thread destroys everything accordingly
	thread_pool control_pool;
	int create_result1 = pool_create_dynamic(&control_pool);
	if(create_result1 < 0) {
		print_create_error(-create_result1);
		return EXIT_FAILURE;
	}

	thread_pool data_pool;
	int create_result2 = pool_create_dynamic(&data_pool);
	if(create_result2 < 0) {
		print_create_error(-create_result2);
		return EXIT_FAILURE;
	}

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	myqueue control_jobIds;
	if(myqueue_init(&control_jobIds) < 0) {
		return EXIT_FAILURE;
	};

	myqueue data_jobIds;
	if(myqueue_init(&data_jobIds) < 0) {
		return EXIT_FAILURE;
	};

	// this is an array of pointers
	ConnectionContext** control_contexts =
	    (ConnectionContext**)malloc(sizeof(ConnectionContext*) * control_pool.workerThreadAmount);

	if(!control_contexts) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	ConnectionContext** data_contexts =
	    (ConnectionContext**)malloc(sizeof(ConnectionContext*) * data_pool.workerThreadAmount);

	if(!data_contexts) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	const SecureOptions* const options = initialize_secure_options(false, "", "");

	for(size_t i = 0; i < control_pool.workerThreadAmount; ++i) {
		control_contexts[i] = get_connection_context(options);
	}

	for(size_t i = 0; i < data_pool.workerThreadAmount; ++i) {
		data_contexts[i] = get_connection_context(options);
	}

	// initializing the thread Arguments for the single listener thread, it receives all
	// necessary arguments
	pthread_t controlListenerThread = {};
	FTPControlThreadArgument control_threadArgument = { .pool = &control_pool,
		                                                .jobIds = &control_jobIds,
		                                                .contexts = control_contexts,
		                                                .socketFd = controlSocketFd,
		                                                .global_folder = folder,
		                                                .ports = { .control = control_port,
		                                                           .data = data_port } };

	pthread_t dataListenerThread = {};
	FTPDataThreadArgument data_threadArgument = { .pool = &data_pool,
		                                          .jobIds = &data_jobIds,
		                                          .contexts = data_contexts,
		                                          .socketFd = dataSocketFd };

	// creating the thread
	result1 = pthread_create(&controlListenerThread, NULL, ftp_control_listener_thread_function,
	                         &control_threadArgument);
	checkForThreadError(result1,
	                    "An Error occurred while trying to create a new control listener Thread",
	                    return EXIT_FAILURE;);

	result2 = pthread_create(&dataListenerThread, NULL, ftp_data_listener_thread_function,
	                         &data_threadArgument);
	checkForThreadError(result2,
	                    "An Error occurred while trying to create a new data listener Thread",
	                    return EXIT_FAILURE;);

	// wait for the single listener thread to finish, that happens when he is cancelled via
	// shutdown request
	ListenerError control_returnValue = ListenerError_None;
	result1 = pthread_join(controlListenerThread, &control_returnValue);
	checkForThreadError(result1, "An Error occurred while trying to wait for a control Thread",
	                    return EXIT_FAILURE;);

	if(is_listener_error(control_returnValue)) {
		if(control_returnValue != ListenerError_None) {
			print_listener_error(control_returnValue);
		}
	} else if(control_returnValue != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "The control listener thread wasn't cancelled properly!\n");
	} else if(control_returnValue == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The control listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError,
		            "The control listener thread was terminated with wrong error: %p!\n",
		            control_returnValue);
	}

	ListenerError data_returnValue = ListenerError_None;
	result2 = pthread_join(dataListenerThread, &data_returnValue);
	checkForThreadError(result1, "An Error occurred while trying to wait for a data Thread",
	                    return EXIT_FAILURE;);

	if(is_listener_error(data_returnValue)) {
		if(data_returnValue != ListenerError_None) {
			print_listener_error(data_returnValue);
		}
	} else if(data_returnValue != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "The data listener thread wasn't cancelled properly!\n");
	} else if(data_returnValue == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The data listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError,
		            "The data listener thread was terminated with wrong error: %p!\n",
		            data_returnValue);
	}

	// since the listener doesn't wait on the jobs, the main thread has to do that work!
	// the queue can be filled, which can lead to a problem!!
	while(!myqueue_is_empty(&control_jobIds)) {
		job_id* jobId = (job_id*)myqueue_pop(&control_jobIds);

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

	while(!myqueue_is_empty(&data_jobIds)) {
		job_id* jobId = (job_id*)myqueue_pop(&data_jobIds);

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
	if(pool_destroy(&control_pool) < 0) {
		return EXIT_FAILURE;
	}

	if(pool_destroy(&data_pool) < 0) {
		return EXIT_FAILURE;
	}

	// then the queue is destroyed
	if(myqueue_destroy(&control_jobIds) < 0) {
		return EXIT_FAILURE;
	}

	if(myqueue_destroy(&data_jobIds) < 0) {
		return EXIT_FAILURE;
	}

	// finally closing the whole socket, so that the port is useable by other programs or by
	// this again, NOTES: ip(7) states :" A TCP local socket address that has been bound is
	// unavailable for  some time after closing, unless the SO_REUSEADDR flag has been set.
	// Care should be taken when using this flag as it makes TCP less reliable." So
	// essentially saying, also correctly closed sockets aren't available after a certain
	// time, even if closed correctly!
	result1 = close(controlSocketFd);
	checkForError(result1, "While trying to close the control socket", return EXIT_FAILURE;);

	result2 = close(dataSocketFd);
	checkForError(result2, "While trying to close the data socket", return EXIT_FAILURE;);

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(control_addr);
	free(data_addr);

	for(size_t i = 0; i < control_pool.workerThreadAmount; ++i) {
		free_connection_context(control_contexts[i]);
	}

	for(size_t i = 0; i < data_pool.workerThreadAmount; ++i) {
		free_connection_context(data_contexts[i]);
	}

	free(folder);

	return EXIT_SUCCESS;
}
