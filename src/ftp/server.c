

#include "./server.h"
#include "./command.h"
#include "./file_ops.h"
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
#include <sys/stat.h>
#include <unistd.h>

#define ANON_USERNAME "anonymous"
#define ALLOW_SSL_AUTO_CONTEXT_REUSE false
#define DEFAULT_PASSIVE_PORT_AMOUNT 10

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

	struct sockaddr_in server_addr_raw;
	socklen_t addr_len = sizeof(server_addr_raw);

	// would be better to set cancel state in the right places!!
	int socknameResult =
	    getsockname(argument->connectionFd, (struct sockaddr*)&server_addr_raw, &addr_len);
	if(socknameResult != 0) {
		LOG_MESSAGE(LogLevelError | LogPrintLocation, "getsockname error: %s\n", strerror(errno));
		return JobError_GetSockName;
	}

	if(addr_len != sizeof(server_addr_raw)) {
		LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "getsockname has wrong addr_len\n");
		return JobError_GetSockName;
	}

	FTPAddrField server_addr = get_port_info_from_sockaddr(server_addr_raw).addr;

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
				LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
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
				LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n");
				goto cleanup;
			}

			continue;
		}

		for(size_t i = 0; i < ftpCommands->size; ++i) {
			FTPCommand* command = ftpCommands->content[i];
			bool successfull = ftp_process_command(descriptor, server_addr, argument, command);
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
	int result =
	    close_connection_descriptor_advanced(descriptor, context, ALLOW_SSL_AUTO_CONTEXT_REUSE);
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
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n"); \
			return false; \
		} \
	} while(false)

#define SEND_RESPONSE_WITH_ERROR_CHECK_F(code, format, ...) \
	do { \
		StringBuilder* sb = string_builder_init(); \
		string_builder_append(sb, return false;, format, __VA_ARGS__); \
		int result = sendFTPMessageToConnectionSb(descriptor, code, sb); \
		if(result < 0) { \
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Error in sending response\n"); \
			return false; \
		} \
	} while(false)

// the timeout is 15 seconds
#define DATA_CONNECTION_WAIT_TIMEOUT_S 15

// the interval is 1,4 seconds
#define DATA_CONNECTION_INTERVAL_NS (NS(2) / 5)
#define DATA_CONNECTION_INTERVAL_S 1

bool ftp_process_command(ConnectionDescriptor* const descriptor, FTPAddrField server_addr,
                         FTPControlConnectionArgument* argument, const FTPCommand* command) {

	FTPState* state = argument->state;

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

		// permission model: everybody that is logged in can use PWD
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

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FTP_RETURN_CODE_DIR_OP_SUCC, "\"%s\"", dirname);

			free(dirname);

			return true;
		}

		case FTP_COMMAND_PASV: {

			FTPPortField reserved_port =
			    get_available_port_for_passive_mode(argument->data_controller);

			if(reserved_port == 0) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED,
				                               "Not entering passive mode. The server has no "
				                               "available ports left, try again later.");
				return true;
			}

			FTPConnectAddr data_addr = { .addr = server_addr, .port = reserved_port };

			char* port_desc = make_address_port_desc(data_addr);

			if(!port_desc) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_SYNTAX_ERROR, "Internal Error!");

				return true;
			}

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FTP_RETURN_CODE_ENTERING_PASSIVE_MODE,
			                                 "Entering Passive Mode %s.", port_desc);

			state->data_settings->mode = FTP_DATA_MODE_PASSIVE;
			state->data_settings->addr = data_addr;

			free(port_desc);

			return true;
		}

		case FTP_COMMAND_PORT: {

			state->data_settings->mode = FTP_DATA_MODE_ACTIVE;
			state->data_settings->addr = *command->data.port_info;

			SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_CMD_OK, "Entering active mode");

			return true;
		}

		// permission model: everybody that is logged in can use LIST
		case FTP_COMMAND_LIST: {

			if(state->account->state != ACCOUNT_STATE_OK) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_NOT_LOGGED_IN,
				                               "Not logged in: can't access files!");

				return true;
			}

			if(state->data_settings->mode == FTP_DATA_MODE_NONE) {
				SEND_RESPONSE_WITH_ERROR_CHECK(
				    FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
				    "No data conenction mode specified, specify either PASSIVE or ACTIVE");

				return true;
			}

			char* arg = command->data.string;

			if(arg == NULL) {
				// A null argument implies the user's current working or default directory.
				arg = ".";
			}

			char* final_file_path = resolve_path_in_cwd(state, arg);

			if(!final_file_path) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
				                               "No such dir");

				return true;
			}

			struct stat stat_result;
			int result = stat(final_file_path, &stat_result);

			if(result != 0) {
				if(errno == ENOENT) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
					                               "No such dir");

					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
				                               "Internal error");

				return true;
			}

			bool is_folder = S_ISDIR(stat_result.st_mode);

			if(access(final_file_path, R_OK) != 0) {
				const char* file_type_str = is_folder ? "folder" : "file";

				SEND_RESPONSE_WITH_ERROR_CHECK_F(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
				                                 "Access to %s denied", file_type_str);

				return true;
			}

			// cleanup old connections, this has to happend, so that old connections for the same
			// client are free 100%, in most of the cases this is a noop
			{
				// empty the data connections and close the ones, that are no longer required or
				// timed out
				ConnectionsToClose* connections_to_close =
				    data_connections_to_close(argument->data_controller);

				if(connections_to_close == NULL) {
					LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation,
					                   "data_connections_to_close failed\n");
				} else {
					for(size_t i = 0; i < connections_to_close->size; ++i) {
						ConnectionDescriptor* connection_to_close =
						    connections_to_close->content[i];
						close_connection_descriptor(connection_to_close);
					}
				}
			}

			DataConnection* data_connection = get_data_connection_for_control_thread_or_add(
			    argument->data_controller, *state->data_settings);

			if(data_connection == NULL) {

				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_DATA_CONNECTION_WAITING_FOR_OPEN,
				                               "Ok. Waiting for data connection");

				// Wait for data connection

				// y2k -> 2038 bug avoidance
				static_assert(sizeof(time_t) == sizeof(uint64_t));
				time_t start_time = time(NULL);

				if(start_time == ((time_t)-1)) {
					LOG_MESSAGE(LogLevelError | LogPrintLocation, "time() failed: %s\n",
					            strerror(errno));
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
					                               "Internal error");
					return true;
				}

				const struct timespec interval = { .tv_nsec = DATA_CONNECTION_INTERVAL_NS,
					                               .tv_sec = DATA_CONNECTION_INTERVAL_S };

				while(true) {
					int sleep_result = nanosleep(&interval, NULL);

					// ignore EINTR errors, as we just want to sleep, if it'S shorter it's not that
					// bad
					if(sleep_result != 0 && errno != EINTR) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
						                               "Internal error");
						return true;
					}

					time_t current_time = time(NULL);

					if(current_time == ((time_t)-1)) {
						LOG_MESSAGE(LogLevelError | LogPrintLocation, "time() failed: %s\n",
						            strerror(errno));
						SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN,
						                               "Internal error");
						return true;
					}

					time_t diff_time = current_time - start_time;

					if(diff_time >= DATA_CONNECTION_WAIT_TIMEOUT_S) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_DATA_CONNECTION_OPEN_ERROR,
						                               "Timeout on waiting for data connection");
					}

					data_connection = get_data_connection_for_control_thread_or_add(
					    argument->data_controller, *state->data_settings);

					if(data_connection != NULL) {
						break;
					}
				}

			} else {
				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_DATA_CONNECTION_ALREADY_OPEN,
				                               "Ok. Sending data");
			}

			// send data
			{

				ConnectionDescriptor* descriptor = data_connection_get_descriptor_to_send_to(
				    argument->data_controller, data_connection);

				if(descriptor == NULL) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_ABORTED,
					                               "Internal error");
					return true;
				}

				SendMode send_mode = get_current_send_mode(state);

				if(send_mode == SEND_MODE_UNSUPPORTED) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_ABORTED,
					                               "Unsupported send mode");
					return true;
				}

				SendData* data_to_send = get_data_to_send_for_list(is_folder, final_file_path);

				if(data_to_send == NULL) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_ABORTED,
					                               "Internal error");
					return true;
				}

				// this is used, so that we can check in between single sends (e.g. in a list), if
				// the user send us a ABORT COMMAND
				SendProgress send_progress = setup_send_progress(data_to_send, send_mode);

				while(!send_progress.finished) {

					if(!send_data_to_send(data_to_send, descriptor, send_mode, &send_progress)) {
						free_send_data(data_to_send);

						SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_ABORTED,
						                               "Internal send error");
						return true;
					}
				}

				free_send_data(data_to_send);

				if(!data_connection_close(argument->data_controller, data_connection)) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_FILE_ACTION_ABORTED,
					                               "Internal error");
					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FTP_RETURN_CODE_CLOSING_DATA_CONNECTION_REQ_OK,
				                               "Success. Closing Data Connection");
			}

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

	set_thread_name("control listener thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting control listener thread\n");

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

			return ListenerError_ThreadAfterCancel;
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

		FTPState* connection_ftp_state = alloc_default_state(argument.global_folder);

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
		connectionArgument->data_controller = argument.data_controller;
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

#define POLL_INTERVALL 5000

anyType(ListenerError*) ftp_data_listener_thread_function(anyType(FTPDataThreadArgument*) arg) {

	set_thread_name("data listener thread");

	FTPDataThreadArgument argument = *((FTPDataThreadArgument*)arg);

	bool success = data_connection_set_port_as_available(argument.data_controller,
	                                                     argument.port_index, argument.port);

	if(!success) {
		LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Failed to set port as available\n");
		return ListenerError_DataController;
	}

#define POLL_FD_AMOUNT 2

#define POLL_SOCKET_ARR_INDEX 0
#define POLL_SIG_ARR_INDEX 1

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[POLL_SOCKET_ARR_INDEX].fd = argument.fd;
	poll_fds[POLL_SOCKET_ARR_INDEX].events = POLLIN;

	sigset_t mySigset;
	sigemptyset(&mySigset);
	sigaddset(&mySigset, SIGINT);
	int sigFd = signalfd(-1, &mySigset, 0);
	// TODO(Totto): don't exit here
	checkForError(sigFd, "While trying to cancel a data listener Thread on signal",
	              exit(EXIT_FAILURE););

	poll_fds[POLL_SIG_ARR_INDEX].fd = sigFd;
	poll_fds[POLL_SIG_ARR_INDEX].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO(Totto): Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socketFd or the signalFd
		int status = 0;
		while(status == 0) {
			status = poll(poll_fds, POLL_FD_AMOUNT, POLL_INTERVALL);
			if(status < 0) {
				LOG_MESSAGE(LogLevelError | LogPrintLocation, "poll failed: %s\n", strerror(errno));
				continue;
			}
		}

		if(poll_fds[POLL_SIG_ARR_INDEX].revents == POLLIN || signal_received != 0) {

			// TODO(Totto): This fd (sigset fd) isn't closed, when pthread_cancel is called from
			// somewhere else, fix that somehow
			close(poll_fds[POLL_SIG_ARR_INDEX].fd);
			int result = pthread_cancel(pthread_self());
			checkForError(result, "While trying to cancel a data listener Thread on signal",
			              return ListenerError_ThreadCancel;);

			return ListenerError_ThreadAfterCancel;
		}

		// the poll didn't see a POLLIN event in the argument.socketFd fd, so the accept
		// will fail, just redo the poll
		if(poll_fds[POLL_SOCKET_ARR_INDEX].revents != POLLIN) {

			// do that here, so that it is done every now and then

			{
				// empty the data connections and close the ones, that are no longer required or
				// timed out
				ConnectionsToClose* connections_to_close =

				    data_connections_to_close(argument.data_controller);

				if(connections_to_close == NULL) {
					LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation,
					                   "data_connections_to_close failed\n");
					continue;
				}

				for(size_t i = 0; i < connections_to_close->size; ++i) {
					ConnectionDescriptor* connection_to_close = connections_to_close->content[i];
					close_connection_descriptor(connection_to_close);
				}
			}

			continue;
		}

		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		// would be better to set cancel state in the right places!!
		int connectionFd = accept(argument.fd, (struct sockaddr*)&client_addr, &addr_len);
		checkForError(connectionFd, "While Trying to accept a socket",
		              return ListenerError_Accept;);

		if(addr_len != sizeof(client_addr)) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "Accept has wrong addr_len\n");
			return ListenerError_Accept;
		}

		LOG_MESSAGE_SIMPLE(LogLevelInfo, "Got a new passive data connection\n");

		DataConnection* data_connection = get_data_connection_for_data_thread_or_add_passive(
		    argument.data_controller, argument.port_index);

		if(data_connection == NULL) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation,
			                   "data_controller_add_entry failed\n");
			return ListenerError_DataController;
		}

		// TODO: get correct context, in future if we use tls
		// TODO: should we also support tls here?
		const SecureOptions* const options = initialize_secure_options(false, "", "");

		ConnectionContext* context = get_connection_context(options);

		ConnectionDescriptor* const descriptor = get_connection_descriptor(context, connectionFd);

		bool success =
		    data_controller_add_descriptor(argument.data_controller, data_connection, descriptor);

		if(!success) {
			LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation, "data_controller_add_fd failed\n");
			return ListenerError_DataController;
		}

		// clean up old ones
		{
			// empty the data connections and close the ones, that are no longer required or
			// timed out
			ConnectionsToClose* connections_to_close =
			    data_connections_to_close(argument.data_controller);

			if(connections_to_close == NULL) {
				LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation,
				                   "data_connections_to_close failed\n");
				continue;
			}

			for(size_t i = 0; i < connections_to_close->size; ++i) {
				ConnectionDescriptor* connection_to_close = connections_to_close->content[i];
				close_connection_descriptor(connection_to_close);
			}
		}
	}

	return ListenerError_None;
}

anyType(ListenerError*)
    ftp_data_orchestrator_thread_function(anyType(FTPDataOrchestratorArgument*) arg) {

	set_thread_name("data orchestrator thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting data orchestrator thread\n");

	FTPDataOrchestratorArgument argument = *((FTPDataOrchestratorArgument*)arg);

	FTPPassivePortStatus* local_port_status_arr =
	    (FTPPassivePortStatus*)malloc(sizeof(FTPPassivePortStatus) * argument.port_amount);

	if(local_port_status_arr == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation,
		                   "Failed to setup passive port array\n");
		return ListenerError_DataController;
	}

	for(size_t i = 0; i < argument.port_amount; ++i) {
		FTPPortField port = argument.ports[i];

		local_port_status_arr[i].port = port;
		local_port_status_arr[i].success = false;

		int sockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		checkForError(sockFd, "While Trying to create a port listening socket", goto cont_outer;);

		// set the reuse port option to the socket, so it can be reused
		const int optval = 1;
		int optionReturn1 = setsockopt(sockFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		checkForError(optionReturn1,
		              "While Trying to set a port listening socket option 'SO_REUSEPORT'",
		              goto cont_outer;);

		struct sockaddr_in* addr =
		    (struct sockaddr_in*)mallocWithMemset(sizeof(struct sockaddr_in), true);

		if(!addr) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			continue;
		}

		addr->sin_family = AF_INET;
		// hto functions are used for networking, since there every number is BIG ENDIAN and
		// linux has Little Endian
		addr->sin_port = htons(port);
		// INADDR_ANY is 0.0.0.0, which means every port, but when nobody forwards it,
		// it means, that by default only localhost can be used to access it
		addr->sin_addr.s_addr = htonl(INADDR_ANY);

		int result1 = bind(sockFd, (struct sockaddr*)addr, sizeof(*addr));
		checkForError(result1, "While trying to bind a port listening socket to port",
		              goto cont_outer;);

		result1 = listen(sockFd, FTP_SOCKET_BACKLOG_SIZE);
		checkForError(result1, "While trying to listen on a port listening socket",
		              goto cont_outer;);

		FTPDataThreadArgument data_threadArgument = {
			.data_controller = argument.data_controller,
			.port = port,
			.port_index = i,
			.fd = sockFd,
		};

		// creating the data thread
		int result2 = pthread_create(&local_port_status_arr[i].thread_ref, NULL,
		                             ftp_data_listener_thread_function, &data_threadArgument);
		checkForThreadError(result2,
		                    "An Error occurred while trying to create a port listening Thread",
		                    goto cont_outer;);

		// a simple trick to label for loops and continue in it
	cont_outer:
		continue;
	}

	bool is_error = false;

	// launched every thread, now wait for them
	for(size_t i = 0; i < argument.port_amount; ++i) {
		FTPPassivePortStatus port_status = local_port_status_arr[i];

		ListenerError returnValue = ListenerError_None;
		int result = pthread_join(port_status.thread_ref, &returnValue);
		checkForThreadError(result,
		                    "An Error occurred while trying to wait for a port listening Thread",
		                    is_error = true;
		                    goto cont_outer2;;);

		if(is_listener_error(returnValue)) {
			if(returnValue != ListenerError_None) {
				print_listener_error(returnValue);
			}
		} else if(returnValue != PTHREAD_CANCELED) {
			LOG_MESSAGE_SIMPLE(LogLevelError,
			                   "A port listener thread wasn't cancelled properly!\n");
		} else if(returnValue == PTHREAD_CANCELED) {
			LOG_MESSAGE_SIMPLE(LogLevelInfo, "A port listener thread was cancelled properly!\n");
		} else {
			LOG_MESSAGE(LogLevelError,
			            "A port  listener thread was terminated with wrong error: %p!\n",
			            returnValue);
		}

	cont_outer2:
		continue;
	}

	free(local_port_status_arr);
	return is_error ? ListenerError_ThreadCancel : ListenerError_None;
}

int startFtpServer(FTPPortField control_port, char* folder) {

	// using TCP  and not 0, which is more explicit about what protocol to use
	// so essentially a socket is created, the protocol is AF_INET alias the IPv4 Prototol,
	// the socket type is SOCK_STREAM, meaning it has reliable read and write capabilities,
	// all other types are not that well suited for that example
	int controlSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	checkForError(controlSocketFd, "While Trying to create control socket", return EXIT_FAILURE;);

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int optionReturn1 =
	    setsockopt(controlSocketFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	checkForError(optionReturn1, "While Trying to set the control socket option 'SO_REUSEPORT'",
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

	control_addr->sin_family = AF_INET;
	// hto functions are used for networking, since there every number is BIG ENDIAN and
	// linux has Little Endian
	control_addr->sin_port = htons(control_port);
	// INADDR_ANY is 0.0.0.0, which means every port, but when nobody forwards it,
	// it means, that by default only localhost can be used to access it
	control_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	// since bind is generic, the specific struct has to be casted, and the actual length
	// has to be given, this is a function signature, just to satisfy the typings, the real
	// requirements are given in the responsive protocol man page, here ip(7) also note that
	// ports below 1024 are  privileged ports, meaning, that you require special permissions
	// to be able to bind to them ( CAP_NET_BIND_SERVICE capability) (the simple way of
	// getting that is being root, or executing as root: sudo ...)
	int result1 = bind(controlSocketFd, (struct sockaddr*)control_addr, sizeof(*control_addr));
	checkForError(result1, "While trying to bind control socket to port", return EXIT_FAILURE;);

	// FTP_SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listening on that socket, meaning
	// new connections can be accepted
	result1 = listen(controlSocketFd, FTP_SOCKET_BACKLOG_SIZE);
	checkForError(result1, "While trying to listen on control socket", return EXIT_FAILURE;);

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

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	myqueue control_jobIds;
	if(myqueue_init(&control_jobIds) < 0) {
		return EXIT_FAILURE;
	};

	// this is an array of pointers
	ConnectionContext** control_contexts =
	    (ConnectionContext**)malloc(sizeof(ConnectionContext*) * control_pool.workerThreadAmount);

	if(!control_contexts) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	const SecureOptions* const options = initialize_secure_options(false, "", "");

	for(size_t i = 0; i < control_pool.workerThreadAmount; ++i) {
		control_contexts[i] = get_connection_context(options);
	}

	size_t port_amount = DEFAULT_PASSIVE_PORT_AMOUNT;

	DataController* data_controller = initialize_data_controller(port_amount);

	if(!data_controller) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	FTPPortField* ports = (FTPPortField*)malloc(port_amount * sizeof(FTPPortField));

	for(size_t i = 0; i < port_amount; ++i) {
		uint32_t next_port = ((uint32_t)control_port) + 1 + i;
		if(next_port > UINT16_MAX) {
			next_port = control_port - (next_port - UINT16_MAX);
		}
		ports[i] = (uint16_t)next_port;
	}

	// initializing the thread Arguments for the single listener thread, it receives all
	// necessary arguments
	pthread_t dataOrchestratorThread = {};
	FTPDataOrchestratorArgument data_threadArgument = { .data_controller = data_controller,
		                                                .port_amount = port_amount,
		                                                .ports = ports };

	// creating the data thread
	int result2 = pthread_create(&dataOrchestratorThread, NULL,
	                             ftp_data_orchestrator_thread_function, &data_threadArgument);
	checkForThreadError(result2,
	                    "An Error occurred while trying to create a new data listener Thread",
	                    return EXIT_FAILURE;);

	pthread_t controlListenerThread = {};
	FTPControlThreadArgument control_threadArgument = { .pool = &control_pool,
		                                                .jobIds = &control_jobIds,
		                                                .contexts = control_contexts,
		                                                .socketFd = controlSocketFd,
		                                                .global_folder = folder,
		                                                .data_controller = data_controller

	};

	// creating the control thread
	result1 = pthread_create(&controlListenerThread, NULL, ftp_control_listener_thread_function,
	                         &control_threadArgument);
	checkForThreadError(result1,
	                    "An Error occurred while trying to create a new control listener Thread",
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
	result2 = pthread_join(dataOrchestratorThread, &data_returnValue);
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

	// then after all were awaited the pool is destroyed
	if(pool_destroy(&control_pool) < 0) {
		return EXIT_FAILURE;
	}

	// then the queue is destroyed
	if(myqueue_destroy(&control_jobIds) < 0) {
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

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(control_addr);

	free(ports);

	for(size_t i = 0; i < control_pool.workerThreadAmount; ++i) {
		free_connection_context(control_contexts[i]);
	}

	free(folder);

	return EXIT_SUCCESS;
}
