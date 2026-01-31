

#include "./server.h"
#include "./command.h"
#include "./file_ops.h"
#include "./protocol.h"
#include "./send.h"
#include "./state.h"

#include "generic/helper.h"
#include "generic/read.h"
#include "generic/send.h"
#include "generic/signal_fd.h"
#include "utils/clock.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/thread_pool.h"
#include "utils/utils.h"

#include <errno.h>
#include <netinet/ip.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define ANON_USERNAME "anonymous"
#define ALLOW_SSL_AUTO_CONTEXT_REUSE false
#define DEFAULT_PASSIVE_PORT_AMOUNT 10

#ifdef _NO_SIGNAL_HANDLER_TYPED_DEFINED
typedef void (*__sighandler_t)(int);
#endif

static bool setup_signal_handler_impl_with_handler(int signal_number, __sighandler_t handle) {
	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = handle;
	// initialize the mask to be empty
	int empty_set_result = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, signal_number);
	int result1 = sigaction(signal_number, &action, NULL);
	if(result1 < 0 || empty_set_result < 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't set signal interception: %s\n", strerror(errno));
		return false;
	}

	return true;
}

static bool setup_signal_handler_impl(int signal_number) {
	return setup_signal_handler_impl_with_handler(signal_number, SIG_IGN);
}

static volatile sig_atomic_t
    g_usr1_signal_received = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    0;

// only setting the volatile sig_atomic_t g_signal_received' in here
static void usr1_handler(int signal_number) {
	g_usr1_signal_received = signal_number;
}

static bool setup_relevant_signal_handlers(void) {

	if(!setup_signal_handler_impl(SIGPIPE)) {
		return false;
	}

	return setup_signal_handler_impl_with_handler(FTP_PASSIVE_DATA_CONNECTION_SIGNAL, usr1_handler);
}

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

ANY_TYPE(JobError*)
ftp_control_socket_connection_handler(ANY_TYPE(FTPControlConnectionArgument*) arg_ign,
                                      WorkerInfo worker_info) {

	// attention arg is malloced!
	FTPControlConnectionArgument* argument = (FTPControlConnectionArgument*)arg_ign;

	ConnectionContext* context =
	    ZVEC_AT(ConnectionContextPtr, argument->contexts, worker_info.worker_index);
	char* thread_name_buffer = NULL;
	FORMAT_STRING(&thread_name_buffer, return JOB_ERROR_STRING_FORMAT;
	              , "connection handler %lu", worker_info.worker_index);
	set_thread_name(thread_name_buffer);

#define FREE_AT_END() \
	do { \
		unset_thread_name(); \
		free(thread_name_buffer); \
		free(argument); \
	} while(false)

	bool signal_result = setup_relevant_signal_handlers();

	if(!signal_result) {
		FREE_AT_END();
		return JOB_ERROR_SIG_HANDLER;
	}

	struct sockaddr_in server_addr_raw;
	socklen_t addr_len = sizeof(server_addr_raw);

	// would be better to set cancel state in the right places!!
	int sockname_result =
	    getsockname(argument->connection_fd, (struct sockaddr*)&server_addr_raw, &addr_len);
	if(sockname_result != 0) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation), "getsockname error: %s\n",
		            strerror(errno));

		FREE_AT_END();
		return JOB_ERROR_GET_SOCK_NAME;
	}

	if(addr_len != sizeof(server_addr_raw)) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
		                   "getsockname has wrong addr_len\n");

		FREE_AT_END();
		return JOB_ERROR_GET_SOCK_NAME;
	}

	FTPAddrField server_addr = get_port_info_from_sockaddr(server_addr_raw).addr;

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting Connection handler\n");

	ConnectionDescriptor* const descriptor =
	    get_connection_descriptor(context, argument->connection_fd);

	if(descriptor == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "get_connection_descriptor failed\n");

		FREE_AT_END();
		return JOB_ERROR_DESC;
	}

	int hello_result = send_ftp_message_to_connection(
	    descriptor, FtpReturnCodeSrvcReady, "Simple FTP Server", ConnectionSendFlagsUnMalloced);
	if(hello_result < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error in sending hello message\n");
		goto cleanup;
	}

	bool quit = false;

	while(!quit) {

		char* raw_ftp_commands = read_string_from_connection(descriptor);

		if(!raw_ftp_commands) {
			int result = send_ftp_message_to_connection(
			    descriptor, FtpReturnCodeSyntaxError,
			    "Request couldn't be read, a connection error occurred!",
			    ConnectionSendFlagsUnMalloced);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
				                   "Error in sending response\n");
				goto cleanup;
			}

			continue;
		}

		// raw_ftp_commands gets freed in here
		FTPCommandArray* ftp_commands = parse_multiple_ftp_commands(raw_ftp_commands);

		// ftp_commands can be null, then it wasn't parse-able, according to parseMultipleCommands,
		// see there for more information
		if(ftp_commands == NULL) {
			int result = send_ftp_message_to_connection(descriptor, FtpReturnCodeSyntaxError,
			                                            "Invalid Command Sequence",
			                                            ConnectionSendFlagsUnMalloced);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
				                   "Error in sending response\n");
				goto cleanup;
			}

			continue;
		}

		for(size_t i = 0; i < ZVEC_LENGTH(*ftp_commands); ++i) {
			FTPCommand* command = ZVEC_AT(FTPCommandPtr, *ftp_commands, i);
			bool successfull = ftp_process_command(descriptor, server_addr, argument, command);
			if(!successfull) {
				quit = true;
				break;
			}
		}

		free_ftp_command_array(ftp_commands);
	}

cleanup:
	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Closing Connection\n");
	// finally close the connection
	int result =
	    close_connection_descriptor_advanced(descriptor, context, ALLOW_SSL_AUTO_CONTEXT_REUSE);
	CHECK_FOR_ERROR(result, "While trying to close the connection descriptor", {
		FREE_AT_END();
		return JOB_ERROR_CLOSE;
	});
	// and free the malloced argument
	FREE_AT_END();
	return JOB_ERROR_NONE;
}

#undef FREE_AT_END

#define SEND_RESPONSE_WITH_ERROR_CHECK(code, msg) \
	do { \
		int send_result = \
		    send_ftp_message_to_connection(descriptor, code, msg, ConnectionSendFlagsUnMalloced); \
		if(send_result < 0) { \
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation), \
			                   "Error in sending response\n"); \
			return false; \
		} \
	} while(false)

#define SEND_RESPONSE_WITH_ERROR_CHECK_F(code, format, ...) \
	do { \
		StringBuilder* string_builder = string_builder_init(); \
		STRING_BUILDER_APPENDF(string_builder, return false;, format, __VA_ARGS__); \
		int send_result = send_ftp_message_to_connection_sb(descriptor, code, string_builder); \
		if(send_result < 0) { \
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation), \
			                   "Error in sending response\n"); \
			return false; \
		} \
	} while(false)

// the timeout is 15 seconds
#define DATA_CONNECTION_WAIT_TIMEOUT_S_D 15.0

// the interval is 1,4 seconds
#define DATA_CONNECTION_INTERVAL_NS (S_TO_NS(2, uint64_t) / 5)
#define DATA_CONNECTION_INTERVAL_S 1

bool ftp_process_command(ConnectionDescriptor* const descriptor, FTPAddrField server_addr,
                         FTPControlConnectionArgument* argument, const FTPCommand* command) {

	FTPState* state = argument->state;

	switch(command->type) {
		case FtpCommandUser: {

			// see https://datatracker.ietf.org/doc/html/rfc1635
			if(strcasecmp(ANON_USERNAME, command->data.string) == 0) {
				free_account_data(state->account);

				state->account->state = AccountStateOk;

				char* malloced_username = copy_cstr(command->data.string);

				if(!malloced_username) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal ERROR!");

					return true;
				}

				AccountOkData ok_data = { .permissions = AccountPermissionsRead,
					                      .username = malloced_username };

				state->account->data.ok_data = ok_data;

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeUserLoggedIn, "Logged In as anon!");

				return true;
			}

			free_account_data(state->account);

			state->account->state = AccountStateOnlyUser;

			char* malloced_username = copy_cstr(command->data.string);

			if(!malloced_username) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal ERROR!");

				return true;
			}

			state->account->data.temp_data.username = malloced_username;

			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNeedPswd, "Need Password!");

			return true;
		}

		case FtpCommandPass: {

			if(state->account->state == AccountStateOk &&
			   strcasecmp(ANON_USERNAME, state->account->data.ok_data.username) == 0) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeUserLoggedIn,
				                               "Already logged in as anon!");

				return true;
			}

			// TODO(Totto): allow user changing
			if(state->account->state != AccountStateOnlyUser) {
				free_account_data(state->account);

				state->account->state = AccountStateEmpty;

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeBadSequence, "No user specified!");

				return true;
			}

			char* username = state->account->data.temp_data.username;

			char* passwd = command->data.string;

			UserValidity user_validity = account_verify(argument->auth_providers, username, passwd);

			switch(user_validity) {
				case UserValidityOk: {

					free_account_data(state->account);

					state->account->state = AccountStateOk;

					char* malloced_username = copy_cstr(username);

					if(!malloced_username) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal ERROR!");

						return true;
					}

					AccountOkData ok_data = { .permissions = AccountPermissionsReadWrite,
						                      .username = malloced_username };

					state->account->data.ok_data = ok_data;

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeUserLoggedIn, "Logged In as user!");

					return true;
				}
				case UserValidityNoSuchUser: {

					free_account_data(state->account);

					state->account->state = AccountStateEmpty;

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn, "No such user found!");

					return true;
				}
				case UserValidityWrongPassword: {
					free_account_data(state->account);

					state->account->state = AccountStateEmpty;

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn, "Wrong password!");

					return true;
				}
				case UserValidityInternalError:
				default: {
					free_account_data(state->account);

					state->account->state = AccountStateEmpty;

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn, "Internal Error!");

					return true;
				}
			}
		}

		// permission model: everybody that is logged in can use PWD
		case FtpCommandPwd: {

			if(state->account->state != AccountStateOk) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn,
				                               "Not logged in: can't access files!");

				return true;
			}

			char* dirname = get_current_dir_name_relative_to_ftp_root(state, true);

			if(!dirname) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal Error!");

				return true;
			}

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FtpReturnCodeDirOpSucc,
			                                 "\"%s\" is the current directory", dirname);

			free(dirname);

			return true;
		}

		// permission model: everybody that is logged in can use CWD
		case FtpCommandCwd: {

			if(state->account->state != AccountStateOk) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn,
				                               "Not logged in: can't access files!");

				return true;
			}

			char* cwd_argument = command->data.string;

			DirChangeResult result = change_dirname_to(state, cwd_argument);

			switch(result) {
				case DirChangeResultOk: {
					break;
				}
				case DirChangeResultErrorPathTraversal: {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTakenFatal,
					                               "Path traversal detected, aborting!");
					return true;
				}
				case DirChangeResultNoSuchDir: {

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTakenFatal,
					                               "No such directory!");
					return true;
				}
				case DirChangeResultError:
				default: {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTakenFatal,
					                               "An unknown error occurred!");
					return true;
				}
			}

			char* dirname = get_current_dir_name_relative_to_ftp_root(state, true);

			if(!dirname) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal Error!");

				return true;
			}

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FtpReturnCodeFileActionOk,
			                                 "directory changed to \"%s\"", dirname);

			free(dirname);

			return true;
		}

		// permission model: everybody that is logged in can use CDUP
		case FtpCommandCdup: {

			if(state->account->state != AccountStateOk) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn,
				                               "Not logged in: can't access files!");

				return true;
			}

			const char* cdup_argument = "..";

			DirChangeResult dir_change_result = change_dirname_to(state, cdup_argument);

			switch(dir_change_result) {
				case DirChangeResultOk: {
					break;
				}
				case DirChangeResultErrorPathTraversal: {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTakenFatal,
					                               "Path traversal detected, aborting!");
					return true;
				}
				case DirChangeResultNoSuchDir: {

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTakenFatal,
					                               "No such directory!");
					return true;
				}
				case DirChangeResultError:
				default: {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTakenFatal,
					                               "An unknown error occurred!");
					return true;
				}
			}

			char* dirname = get_current_dir_name_relative_to_ftp_root(state, true);

			if(!dirname) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal Error!");

				return true;
			}

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FtpReturnCodeCmdOk, "directory changed to \"%s\"",
			                                 dirname);

			free(dirname);

			return true;
		}

		case FtpCommandPasv: {

			FTPPortField reserved_port =
			    get_available_port_for_passive_mode(argument->data_controller);

			if(reserved_port == 0) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCommandNotImplemented,
				                               "Not entering passive mode. The server has no "
				                               "available ports left, try again later.");
				return true;
			}

			FTPConnectAddr data_addr = { .addr = server_addr, .port = reserved_port };

			char* port_desc = make_address_port_desc(data_addr);

			if(!port_desc) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal Error!");

				return true;
			}

			SEND_RESPONSE_WITH_ERROR_CHECK_F(FtpReturnCodeEnteringPassiveMode,
			                                 "Entering Passive Mode %s.", port_desc);

			state->data_settings->mode = FtpDataModePassive;
			state->data_settings->addr = data_addr;

			free(port_desc);

			return true;
		}

		case FtpCommandFeat: {

			if(state->supported_features->size == 0) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFeatureList,
				                               "No additional features supported");

				return true;
			}

			// send start
			{
				StringBuilder* string_builder = string_builder_init();

				if(!string_builder) {
					LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					                   "Error in sending start feature response\n");
					return false;
				}

				STRING_BUILDER_APPENDF(string_builder, return false;
				                       , "%03d-Extensions supported:", FtpReturnCodeFeatureList);
				int send_result = send_string_builder_to_connection(descriptor, &string_builder);
				if(send_result < 0) {
					LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					                   "Error in sending start feature response\n");
					return false;
				}
			}
			for(size_t i = 0; state->supported_features->size; ++i) {
				FTPSupportedFeature feature = state->supported_features->features[i];

				StringBuilder* string_builder = string_builder_init();

				if(!string_builder) {
					LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					                   "Error in sending manual feature response\n");
					return false;
				}

				STRING_BUILDER_APPENDF(string_builder, return false;, " %s", feature.name);

				if(feature.arguments != NULL) {
					STRING_BUILDER_APPENDF(string_builder, return false;, " %s", feature.arguments);
				}

				int send_result = send_string_builder_to_connection(descriptor, &string_builder);
				if(send_result < 0) {
					LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					                   "Error in sending manual feature response\n");
					return false;
				}
			}

			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFeatureList, "END");

			return true;
		}

		case FtpCommandPort: {

			state->data_settings->mode = FtpDataModeActive;
			state->data_settings->addr = *command->data.port_info;

			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCmdOk, "Entering active mode");

			return true;
		}

			// TODO(Totto): deduplicate LIST / RETR / STORand file commands
		// permission model: you have to be logged in and have WRITE Permissions
		case FtpCommandStor: {

			if(state->account->state != AccountStateOk) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn,
				                               "Not logged in: can't upload files!");

				return true;
			}

			/* if((state->account->data.ok_data.permissions & AccountPermissionsWrite) == 0) {
			    SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNedAcctForStore,
			                                   "No write permissions with this user!");

			    return true;
			} */

			if(state->data_settings->mode == FtpDataModeNone) {
				SEND_RESPONSE_WITH_ERROR_CHECK(
				    FtpReturnCodeFileActionNotTaken,
				    "No data conenction mode specified, specify either PASSIVE or ACTIVE");

				return true;
			}

			char* arg = command->data.string;

			// NOTE: we allow overwrites, as the ftp spec says

			char* final_file_path = resolve_path_in_cwd(state, arg);

			if(!final_file_path) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
				                               "Path resolve error");

				return true;
			}

			struct stat stat_result;
			int result = stat(final_file_path, &stat_result);

			if(result != 0) {
				if(errno != ENOENT) {

					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Internal error 1");
					return true;
				}
			} else {

				bool is_folder = S_ISDIR(stat_result.st_mode);

				if(is_folder) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Can't overwrite a folder");

					return true;
				}
			}

			// cleanup old connections, this has to happend, so that old connections for the
			// same client are free 100%, in most of the cases this is a noop
			{
				// empty the data connections and close the ones, that are no longer required or
				// timed out
				ConnectionsToClose* connections_to_close =
				    data_connections_to_close(argument->data_controller);

				for(size_t i = 0; i < ZVEC_LENGTH(*connections_to_close); ++i) {
					ConnectionDescriptor* connection_to_close =
					    ZVEC_AT(ConnectionDescriptorPtr, *connections_to_close, i);
					close_connection_descriptor(connection_to_close);
				}
				ZVEC_FREE(ConnectionDescriptorPtr, connections_to_close);
			}

			DataConnection* data_connection = get_data_connection_for_control_thread_or_add(
			    argument->data_controller, *state->data_settings);

			if(data_connection == NULL) {

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionWaitingForOpen,
				                               "Ok. Waiting for data connection");

				// Wait for data connection

				Time start_time;
				bool clock_result = get_monotonic_time(&start_time);

				if(!clock_result) {
					LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					            "time() failed: %s\n", strerror(errno));
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Internal error 2");
					return true;
				}

				const struct timespec interval = { .tv_nsec = DATA_CONNECTION_INTERVAL_NS,
					                               .tv_sec = DATA_CONNECTION_INTERVAL_S };

				// TODO(Totto): don't use intervals for active connection, use poll() for that!
				while(true) {

					if(g_usr1_signal_received == 0) {
						int sleep_result = nanosleep(&interval, NULL);

						// ignore EINTR errors, as we just want to sleep, if it'S shorter it's
						// not that bad, we also interrupt this thread in passive mode, so that
						// we are faster, than waiting a fixed amount
						if(sleep_result != 0 && errno != EINTR) {
							SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
							                               "Internal error 3");
							return true;
						}

						g_usr1_signal_received = 0;
					}

					Time current_time;
					bool clock_result_2 = get_monotonic_time(&current_time);

					if(!clock_result_2) {
						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
						            "getting the time failed: %s\n", strerror(errno));
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
						                               "Internal error 4");
						return true;
					}

					double diff_time = time_diff_in_exact_seconds(current_time, start_time);

					if(diff_time >= DATA_CONNECTION_WAIT_TIMEOUT_S_D) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionOpenError,
						                               "Timeout on waiting for data connection");

						return true;
					}

					data_connection = get_data_connection_for_control_thread_or_add(
					    argument->data_controller, *state->data_settings);

					if(data_connection != NULL) {

						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintLocation),
						            "Data connection established after %f s\n", diff_time);

						break;
					}
				}
			} else {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionAlreadyOpen,
				                               "Ok. Sending data");

				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintLocation),
				                   "Data connection already established\n");
			}

			// retrieve data
			{

				ConnectionDescriptor* data_conn_descriptor =
				    data_connection_get_descriptor_to_send_to(argument->data_controller,
				                                              data_connection);

				char* resulting_data = read_string_from_connection(data_conn_descriptor);

				if(!resulting_data) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Internal error 5");
					return true;
				}

				size_t data_length = strlen(resulting_data);

				// store data
				bool success = write_to_file(final_file_path, resulting_data, data_length);

				if(!success) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Internal error 6");
					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeClosingDataConnectionReqOk,
				                               "Success. Closing Data Connection");
			}

			return true;
		}

			// permission model: everybody that is logged in can use RETR
		case FtpCommandRetr: {

			if(state->account->state != AccountStateOk) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn,
				                               "Not logged in: can't access files!");

				return true;
			}

			if(state->data_settings->mode == FtpDataModeNone) {
				SEND_RESPONSE_WITH_ERROR_CHECK(
				    FtpReturnCodeFileActionNotTaken,
				    "No data conenction mode specified, specify either PASSIVE or ACTIVE");

				return true;
			}

			char* arg = command->data.string;

			char* final_file_path = resolve_path_in_cwd(state, arg);

			if(!final_file_path) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
				                               "Path resolve error");

				return true;
			}

			struct stat stat_result;
			int result = stat(final_file_path, &stat_result);

			if(result != 0) {
				if(errno == ENOENT) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "No such file / dir");

					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken, "Internal error 7");

				return true;
			}

			bool is_folder = S_ISDIR(stat_result.st_mode);

			if(is_folder) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
				                               "Can't RETR a folder");

				return true;
			}

			if(access(final_file_path, R_OK) != 0) {

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
				                               "Access to file denied");

				return true;
			}

			// cleanup old connections, this has to happend, so that old connections for the
			// same client are free 100%, in most of the cases this is a noop
			{
				// empty the data connections and close the ones, that are no longer required or
				// timed out
				ConnectionsToClose* connections_to_close =
				    data_connections_to_close(argument->data_controller);

				for(size_t i = 0; i < ZVEC_LENGTH(*connections_to_close); ++i) {
					ConnectionDescriptor* connection_to_close =
					    ZVEC_AT(ConnectionDescriptorPtr, *connections_to_close, i);
					close_connection_descriptor(connection_to_close);
				}
				ZVEC_FREE(ConnectionDescriptorPtr, connections_to_close);
			}

			DataConnection* data_connection = get_data_connection_for_control_thread_or_add(
			    argument->data_controller, *state->data_settings);

			if(data_connection == NULL) {

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionWaitingForOpen,
				                               "Ok. Waiting for data connection");

				// Wait for data connection

				Time start_time;
				bool clock_result = get_monotonic_time(&start_time);

				if(!clock_result) {
					LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					            "time() failed: %s\n", strerror(errno));
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Internal error 8");
					return true;
				}

				const struct timespec interval = { .tv_nsec = DATA_CONNECTION_INTERVAL_NS,
					                               .tv_sec = DATA_CONNECTION_INTERVAL_S };

				// TODO(Totto): don't use intervals for active connection, use poll() for that!
				while(true) {

					if(g_usr1_signal_received == 0) {
						int sleep_result = nanosleep(&interval, NULL);

						// ignore EINTR errors, as we just want to sleep, if it'S shorter it's
						// not that bad, we also interrupt this thread in passive mode, so that
						// we are faster, than waiting a fixed amount
						if(sleep_result != 0 && errno != EINTR) {
							SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
							                               "Internal error 9");
							return true;
						}

						g_usr1_signal_received = 0;
					}

					Time current_time;
					bool clock_result_2 = get_monotonic_time(&current_time);

					if(!clock_result_2) {
						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
						            "getting the time failed: %s\n", strerror(errno));
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
						                               "Internal error 10");
						return true;
					}

					double diff_time = time_diff_in_exact_seconds(current_time, start_time);

					if(diff_time >= DATA_CONNECTION_WAIT_TIMEOUT_S_D) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionOpenError,
						                               "Timeout on waiting for data connection");

						return true;
					}

					data_connection = get_data_connection_for_control_thread_or_add(
					    argument->data_controller, *state->data_settings);

					if(data_connection != NULL) {

						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintLocation),
						            "Data connection established after %f s\n", diff_time);

						break;
					}
				}
			} else {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionAlreadyOpen,
				                               "Ok. Sending data");

				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintLocation),
				                   "Data connection already established\n");
			}

			// send data
			{

				ConnectionDescriptor* data_conn_descriptor =
				    data_connection_get_descriptor_to_send_to(argument->data_controller,
				                                              data_connection);

				if(descriptor == NULL) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Internal error: 11");
					return true;
				}

				SendMode send_mode = get_current_send_mode(state);

				if(send_mode == SendModeUnsupported) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Unsupported send mode");
					return true;
				}

				SendData* data_to_send = get_data_to_send_for_retr(final_file_path);

				if(data_to_send == NULL) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Internal error 12");
					return true;
				}

				// this is used, so that we can check in between single sends (e.g. in a list),
				// if the user send us a ABORT COMMAND
				SendProgress* send_progress = setup_send_progress(data_to_send, send_mode);

				while(!send_progress_is_finished(send_progress)) {

					if(!send_data_to_send(data_to_send, data_conn_descriptor, send_mode,
					                      send_progress)) {
						free_send_data(data_to_send);

						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
						                               "Internal send error");
						return true;
					}
				}

				free_send_data(data_to_send);
				free_send_progress(send_progress);

				if(!data_connection_close(argument->data_controller, data_connection)) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Internal error 13");
					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeClosingDataConnectionReqOk,
				                               "Success. Closing Data Connection");
			}

			return true;
		}

		// permission model: everybody that is logged in can use LIST
		case FtpCommandList: {

			if(state->account->state != AccountStateOk) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeNotLoggedIn,
				                               "Not logged in: can't access files!");

				return true;
			}

			if(state->data_settings->mode == FtpDataModeNone) {
				SEND_RESPONSE_WITH_ERROR_CHECK(
				    FtpReturnCodeFileActionNotTaken,
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
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
				                               "Path resolve error");

				return true;
			}

			struct stat stat_result;
			int stat_ret = stat(final_file_path, &stat_result);

			if(stat_ret != 0) {
				if(errno == ENOENT) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "No such file / dir");

					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
				                               "Internal error 14");

				return true;
			}

			bool is_folder = S_ISDIR(stat_result.st_mode);

			if(access(final_file_path, R_OK) != 0) {
				const char* file_type_str =
				    is_folder ? "folder" : "file"; // NOLINT(readability-implicit-bool-conversion)

				SEND_RESPONSE_WITH_ERROR_CHECK_F(FtpReturnCodeFileActionNotTaken,
				                                 "Access to %s denied", file_type_str);

				return true;
			}

			// cleanup old connections, this has to happend, so that old connections for the
			// same client are free 100%, in most of the cases this is a noop
			{
				// empty the data connections and close the ones, that are no longer required or
				// timed out
				ConnectionsToClose* connections_to_close =
				    data_connections_to_close(argument->data_controller);

				for(size_t i = 0; i < ZVEC_LENGTH(*connections_to_close); ++i) {
					ConnectionDescriptor* connection_to_close =
					    ZVEC_AT(ConnectionDescriptorPtr, *connections_to_close, i);
					close_connection_descriptor(connection_to_close);
				}
				ZVEC_FREE(ConnectionDescriptorPtr, connections_to_close);
			}

			DataConnection* data_connection = get_data_connection_for_control_thread_or_add(
			    argument->data_controller, *state->data_settings);

			if(data_connection == NULL) {

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionWaitingForOpen,
				                               "Ok. Waiting for data connection");

				// Wait for data connection

				Time start_time;
				bool clock_result = get_monotonic_time(&start_time);

				if(!clock_result) {
					LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
					            "time() failed: %s\n", strerror(errno));
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
					                               "Internal error 15");
					return true;
				}

				const struct timespec interval = { .tv_nsec = DATA_CONNECTION_INTERVAL_NS,
					                               .tv_sec = DATA_CONNECTION_INTERVAL_S };

				// TODO(Totto): don't use intervals for active connection, use poll() for that!
				while(true) {

					if(g_usr1_signal_received == 0) {
						int sleep_result = nanosleep(&interval, NULL);

						// ignore EINTR errors, as we just want to sleep, if it'S shorter it's
						// not that bad, we also interrupt this thread in passive mode, so that
						// we are faster, than waiting a fixed amount
						if(sleep_result != 0 && errno != EINTR) {
							SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
							                               "Internal error 16");
							return true;
						}

						g_usr1_signal_received = 0;
					}

					Time current_time;
					bool clock_result_2 = get_monotonic_time(&current_time);

					if(!clock_result_2) {
						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
						            "getting the time failed: %s\n", strerror(errno));
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionNotTaken,
						                               "Internal  17");
						return true;
					}

					double diff_time = time_diff_in_exact_seconds(current_time, start_time);

					if(diff_time >= DATA_CONNECTION_WAIT_TIMEOUT_S_D) {
						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionOpenError,
						                               "Timeout on waiting for data connection");

						return true;
					}

					data_connection = get_data_connection_for_control_thread_or_add(
					    argument->data_controller, *state->data_settings);

					if(data_connection != NULL) {

						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintLocation),
						            "Data connection established after %f s\n", diff_time);

						break;
					}
				}
			} else {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeDataConnectionAlreadyOpen,
				                               "Ok. Sending data");

				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintLocation),
				                   "Data connection already established\n");
			}

			// send data
			{

				ConnectionDescriptor* data_conn_descriptor =
				    data_connection_get_descriptor_to_send_to(argument->data_controller,
				                                              data_connection);

				if(descriptor == NULL) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Internal error 17");
					return true;
				}

				SendMode send_mode = get_current_send_mode(state);

				if(send_mode == SendModeUnsupported) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Unsupported send mode");
					return true;
				}

				SendData* data_to_send = get_data_to_send_for_list(is_folder, final_file_path,
				                                                   state->options->send_format);

				if(data_to_send == NULL) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Internal error 18");
					return true;
				}

				// this is used, so that we can check in between single sends (e.g. in a list),
				// if the user send us a ABORT COMMAND
				SendProgress* send_progress = setup_send_progress(data_to_send, send_mode);

				while(!send_progress_is_finished(send_progress)) {

					if(!send_data_to_send(data_to_send, data_conn_descriptor, send_mode,
					                      send_progress)) {
						free_send_data(data_to_send);

						SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
						                               "Internal send error");
						return true;
					}
				}

				free_send_data(data_to_send);
				free_send_progress(send_progress);

				if(!data_connection_close(argument->data_controller, data_connection)) {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeFileActionAborted,
					                               "Internal error 19");
					return true;
				}

				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeClosingDataConnectionReqOk,
				                               "Success. Closing Data Connection");
			}

			return true;
		}

		case FtpCommandType: {

			FTPCommandTypeInformation* type_info = command->data.type_info;
			if(!type_info->is_normal) {
				SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCommandNotImplementedForParam,
				                               "Not Implemented!");
				return true;
			}

			switch(type_info->data.type & FtpTransmissionTypeMaskBase) {
				case FtpTransmissionTypeAscii: {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError,
					                               "ASCII type not supported atm!");

					return false;
					// state->current_type = FtpTransmissionTypeAscii;
					// break;
				}
				case FtpTransmissionTypeEbcdic: {
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError,
					                               "EBCDIC type not supported atm!");

					return false;
					// state->current_type = FtpTransmissionTypeEbcdic;
					// break;
				}
				case FtpTransmissionTypeImage: {
					state->current_type = FtpTransmissionTypeImage;
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCmdOk, "Set Type To Binary!");
					return true;
				}
				default:
					SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSyntaxError, "Internal ERROR!");

					return true;
			}

			// TODO(Totto): also handle flags
			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCmdOk, "Set Type!");

			return true;
		}

		case FtpCommandAuth: {
#ifdef _SIMPLE_SERVER_SECURE_DISABLED
			SEND_RESPONSE_WITH_ERROR_CHECK(
			    FtpReturnCodeCommandNotImplemented,
			    "AUTH not supported, server not build with ssl / tls enabled!");
			return true;
#else

			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCommandNotImplemented,
			                               "AUTH recognized, but command not implemented!");

			return true;

#endif
		}

		case FtpCommandSyst:

		{
			// see e.g: https://cr.yp.to/ftp/syst.html
			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeSystemName, "UNIX Type: L8 Version: Linux");

			return true;
		}

		case FtpCommandNoop:

		{
			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCmdOk, "");

			return true;
		}

		default: {
			LOG_MESSAGE(LogLevelWarn, "Command %s not implemented\n", get_command_name(command));
			SEND_RESPONSE_WITH_ERROR_CHECK(FtpReturnCodeCommandNotImplemented,
			                               "Command not implemented!");

			return true;
		}
	}
}

// implemented specifically for the ftp Server, it just gets the internal value, but it's better
// to not access that, since additional steps can be required, like  boundary checks!
static int myqueue_size(Myqueue* queue) {
	if(queue->size < 0) {
		LOG_MESSAGE(LogLevelCritical,
		            "internal size implementation error in the queue, value negative: %d!",
		            queue->size);
	}
	return queue->size;
}

static volatile sig_atomic_t
    g_signal_received = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    0;

// only setting the volatile sig_atomic_t g_signal_received' in here
static void receive_signal(int signal_number) {
	g_signal_received = signal_number;
}

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
ANY_TYPE(ListenerError*)
ftp_control_listener_thread_function(ANY_TYPE(FTPControlThreadArgument*) arg) {

	set_thread_name("control listener thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting control listener thread\n");

	FTPControlThreadArgument argument = *((FTPControlThreadArgument*)arg);

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
			// TODO(Totto): This fd isn't closed, when pthread_cancel is called from somewhere
			// else, fix that somehow
			close(poll_fds[1].fd);
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

		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		// would be better to set cancel state in the right places!!
		int connection_fd = accept(argument.socket_fd, (struct sockaddr*)&client_addr, &addr_len);
		CHECK_FOR_ERROR(connection_fd, "While Trying to accept a socket",
		                return LISTENER_ERROR_ACCEPT;);

		if(addr_len != sizeof(client_addr)) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
			                   "Accept has wrong addr_len\n");
			return LISTENER_ERROR_ACCEPT;
		}

		FTPControlConnectionArgument* connection_argument =
		    (FTPControlConnectionArgument*)malloc(sizeof(FTPControlConnectionArgument));

		if(!connection_argument) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
			                   "Couldn't allocate memory!\n");
			return LISTENER_ERROR_MALLOC;
		}

		FTPState* connection_ftp_state = alloc_default_state(argument.global_folder);

		if(!connection_ftp_state) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
			                   "Couldn't allocate memory!\n");
			free(connection_argument);
			return LISTENER_ERROR_MALLOC;
		}

		// to have longer lifetime, that is needed here, since otherwise it would be "dead"
		connection_argument->contexts = argument.contexts;
		connection_argument->listener_thread = pthread_self();
		connection_argument->connection_fd = connection_fd;
		connection_argument->state = connection_ftp_state;
		connection_argument->addr = client_addr;
		connection_argument->data_controller = argument.data_controller;
		connection_argument->data_orchestrator = argument.data_orchestrator;
		connection_argument->auth_providers = argument.auth_providers;

		// push to the queue, but not await, since when we wait it wouldn't be fast and
		// ready to accept new connections
		if(myqueue_push(argument.job_ids,
		                pool_submit(argument.pool, ftp_control_socket_connection_handler,
		                            connection_argument)) < 0) {
			return LISTENER_ERROR_QUEUE_PUSH;
		}

		// not waiting directly, but when the queue grows to fast, it is reduced, then the
		// listener thread MIGHT block, but probably are these first jobs already finished,
		// so its super fast,but if not doing that, the queue would overflow, nothing in
		// here is a cancellation point, so it's safe to cancel here, since only accept then
		// really cancels
		int size = myqueue_size(argument.job_ids);
		if(size > FTP_MAX_QUEUE_SIZE) {
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
}

#define POLL_INTERVALL 5000

ANY_TYPE(ListenerError*) ftp_data_listener_thread_function(ANY_TYPE(FTPDataThreadArgument*) arg) {

	set_thread_name("data listener thread");

	FTPDataThreadArgument argument = *((FTPDataThreadArgument*)arg);

	bool success = data_connection_set_port_as_available(argument.data_controller,
	                                                     argument.port_index, argument.port);

	if(!success) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
		                   "Failed to set port as available\n");
		return LISTENER_ERROR_DATA_CONTROLLER;
	}

#define POLL_FD_AMOUNT 2

#define POLL_SOCKET_ARR_INDEX 0
#define POLL_SIG_ARR_INDEX 1

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[POLL_SOCKET_ARR_INDEX].fd = argument.fd;
	poll_fds[POLL_SOCKET_ARR_INDEX].events = POLLIN;

	int sig_fd = get_signal_like_fd(SIGINT);
	// TODO(Totto): don't exit here
	CHECK_FOR_ERROR(sig_fd, "While trying to cancel a data listener Thread on signal",
	                exit(EXIT_FAILURE););

	poll_fds[POLL_SIG_ARR_INDEX].fd = sig_fd;
	poll_fds[POLL_SIG_ARR_INDEX].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO(Totto): Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socket_fd or the signalFd
		int status = 0;
		while(status == 0) {
			status = poll(poll_fds, POLL_FD_AMOUNT, POLL_INTERVALL);
			if(status < 0) {
				LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation), "poll failed: %s\n",
				            strerror(errno));
				continue;
			}
		}

		if(poll_fds[POLL_SIG_ARR_INDEX].revents == POLLIN || g_signal_received != 0) {

			// TODO(Totto): This fd (sigset fd) isn't closed, when pthread_cancel is called from
			// somewhere else, fix that somehow
			close(poll_fds[POLL_SIG_ARR_INDEX].fd);
			int result = pthread_cancel(pthread_self());
			CHECK_FOR_ERROR(result, "While trying to cancel a data listener Thread on signal",
			                return LISTENER_ERROR_THREAD_CANCEL;);

			return LISTENER_ERROR_THREAD_AFTER_CANCEL;
		}

		// the poll didn't see a POLLIN event in the argument.socket_fd fd, so the accept
		// will fail, just redo the poll
		if(poll_fds[POLL_SOCKET_ARR_INDEX].revents != POLLIN) {

			// do that here, so that it is done every now and then

			{
				// empty the data connections and close the ones, that are no longer required or
				// timed out
				ConnectionsToClose* connections_to_close =
				    data_connections_to_close(argument.data_controller);

				for(size_t i = 0; i < ZVEC_LENGTH(*connections_to_close); ++i) {
					ConnectionDescriptor* connection_to_close =
					    ZVEC_AT(ConnectionDescriptorPtr, *connections_to_close, i);
					close_connection_descriptor(connection_to_close);
				}
				ZVEC_FREE(ConnectionDescriptorPtr, connections_to_close);
			}

			continue;
		}

		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		// would be better to set cancel state in the right places!!
		int connection_fd = accept(argument.fd, (struct sockaddr*)&client_addr, &addr_len);
		CHECK_FOR_ERROR(connection_fd, "While Trying to accept a socket",
		                return LISTENER_ERROR_ACCEPT;);

		if(addr_len != sizeof(client_addr)) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
			                   "Accept has wrong addr_len\n");
			return LISTENER_ERROR_ACCEPT;
		}

		LOG_MESSAGE_SIMPLE(LogLevelInfo, "Got a new passive data connection\n");

		DataConnection* data_connection = get_data_connection_for_data_thread_or_add_passive(
		    argument.data_controller, argument.port_index);

		if(data_connection == NULL) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
			                   "get_data_connection_for_data_thread_or_add_passive failed\n");
			return LISTENER_ERROR_DATA_CONTROLLER;
		}

		// TODO(Totto): get correct context, in future if we use tls
		// TODO(Totto): should we also support tls here? Answer, there is a separate FTP rfc
		// extension to set the encryption state of the data connection, per default it is off
		const SecureOptions* const options = initialize_secure_options(false, "", "");

		ConnectionContext* context = get_connection_context(options);

		ConnectionDescriptor* const descriptor = get_connection_descriptor(context, connection_fd);

		bool add_success =
		    data_controller_add_descriptor(argument.data_controller, data_connection, descriptor);

		if(!add_success) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
			                   "data_controller_add_descriptor failed\n");
			return LISTENER_ERROR_DATA_CONTROLLER;
		}

		// clean up old ones
		{
			// empty the data connections and close the ones, that are no longer required or
			// timed out
			ConnectionsToClose* connections_to_close =
			    data_connections_to_close(argument.data_controller);

			for(size_t i = 0; i < ZVEC_LENGTH(*connections_to_close); ++i) {
				ConnectionDescriptor* connection_to_close =
				    ZVEC_AT(ConnectionDescriptorPtr, *connections_to_close, i);
				close_connection_descriptor(connection_to_close);
			}
			ZVEC_FREE(ConnectionDescriptorPtr, connections_to_close);
		}
	}

	return LISTENER_ERROR_NONE;
}

ANY_TYPE(ListenerError*)
ftp_data_orchestrator_thread_function(ANY_TYPE(FTPDataOrchestratorArgument*) arg) {

	set_thread_name("data orchestrator thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting data orchestrator thread\n");

	FTPDataOrchestratorArgument argument = *((FTPDataOrchestratorArgument*)arg);

	FTPPassivePortStatus* local_port_status_arr =
	    (FTPPassivePortStatus*)malloc(sizeof(FTPPassivePortStatus) * argument.port_amount);

	if(local_port_status_arr == NULL) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
		                   "Failed to setup passive port array\n");
		return LISTENER_ERROR_DATA_CONTROLLER;
	}

	for(size_t i = 0; i < argument.port_amount; ++i) {
		FTPPortField port = argument.ports[i];

		local_port_status_arr[i].port = port;
		local_port_status_arr[i].success = false;

		int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		CHECK_FOR_ERROR(sock_fd, "While Trying to create a port listening socket",
		                goto cont_outer;);

		// set the reuse port option to the socket, so it can be reused
		const int optval = 1;
		int option_return1 = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		CHECK_FOR_ERROR(option_return1,
		                "While Trying to set a port listening socket option 'SO_REUSEPORT'",
		                goto cont_outer;);

		struct sockaddr_in* addr =
		    (struct sockaddr_in*)malloc_with_memset(sizeof(struct sockaddr_in), true);

		if(!addr) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
			                   "Couldn't allocate memory!\n");
			continue;
		}

		addr->sin_family = AF_INET;
		// hto functions are used for networking, since there every number is BIG ENDIAN and
		// linux has Little Endian
		addr->sin_port = htons(port);
		// INADDR_ANY is 0.0.0.0, which means every port, but when nobody forwards it,
		// it means, that by default only localhost can be used to access it
		addr->sin_addr.s_addr = htonl(INADDR_ANY);

		int result1 = bind(sock_fd, (struct sockaddr*)addr, sizeof(*addr));
		CHECK_FOR_ERROR(result1, "While trying to bind a port listening socket to port",
		                goto cont_outer;);

		result1 = listen(sock_fd, FTP_SOCKET_BACKLOG_SIZE);
		CHECK_FOR_ERROR(result1, "While trying to listen on a port listening socket",
		                goto cont_outer;);

		FTPDataThreadArgument data_thread_argument = {
			.data_controller = argument.data_controller,
			.port = port,
			.port_index = i,
			.fd = sock_fd,
		};

		// creating the data thread
		int result2 = pthread_create(&local_port_status_arr[i].thread_ref, NULL,
		                             ftp_data_listener_thread_function, &data_thread_argument);
		CHECK_FOR_THREAD_ERROR(result2,
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

		ListenerError return_value = LISTENER_ERROR_NONE;
		int result = pthread_join(port_status.thread_ref, &return_value);
		CHECK_FOR_THREAD_ERROR(result,
		                       "An Error occurred while trying to wait for a port listening Thread",
		                       is_error = true;
		                       goto cont_outer2;;);

		if(is_listener_error(return_value)) {
			if(return_value != LISTENER_ERROR_NONE) {
				print_listener_error(return_value);
			}
		} else if(return_value != PTHREAD_CANCELED) {
			LOG_MESSAGE_SIMPLE(LogLevelError,
			                   "A port listener thread wasn't cancelled properly!\n");
		} else if(return_value == PTHREAD_CANCELED) {
			LOG_MESSAGE_SIMPLE(LogLevelInfo, "A port listener thread was cancelled properly!\n");
		} else {
			LOG_MESSAGE(LogLevelError,
			            "A port  listener thread was terminated with wrong error: %p!\n",
			            return_value);
		}

	cont_outer2:
		continue;
	}

	free(local_port_status_arr);
	return is_error ? LISTENER_ERROR_THREAD_CANCEL // NOLINT(readability-implicit-bool-conversion)
	                : LISTENER_ERROR_NONE;
}

int start_ftp_server(FTPPortField control_port, char* folder, SecureOptions* options,
                     AuthenticationProviders* const auth_providers) {

	// using TCP  and not 0, which is more explicit about what protocol to use
	// so essentially a socket is created, the protocol is AF_INET alias the IPv4 Prototol,
	// the socket type is SOCK_STREAM, meaning it has reliable read and write capabilities,
	// all other types are not that well suited for that example
	int control_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	CHECK_FOR_ERROR(control_socket_fd, "While Trying to create control socket",
	                return EXIT_FAILURE;);

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int option_return1 =
	    setsockopt(control_socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	CHECK_FOR_ERROR(option_return1, "While Trying to set the control socket option 'SO_REUSEPORT'",
	                return EXIT_FAILURE;);

	// creating the sockaddr_in struct, each number that is used in context of network has
	// to be converted into ntework byte order (Big Endian, linux uses Little Endian) that
	// is relevant for each multibyte value, essentially everything but char, so htox is
	// used, where x stands for different lengths of numbers, s for int, l for long
	struct sockaddr_in* control_addr =
	    (struct sockaddr_in*)malloc_with_memset(sizeof(struct sockaddr_in), true);

	if(!control_addr) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
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
	int result1 = bind(control_socket_fd, (struct sockaddr*)control_addr, sizeof(*control_addr));
	CHECK_FOR_ERROR(result1, "While trying to bind control socket to port", return EXIT_FAILURE;);

	// FTP_SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listening on that socket, meaning
	// new connections can be accepted
	result1 = listen(control_socket_fd, FTP_SOCKET_BACKLOG_SIZE);
	CHECK_FOR_ERROR(result1, "While trying to listen on control socket", return EXIT_FAILURE;);

	LOG_MESSAGE(LogLevelInfo, "To use this simple FTP Server visit ftp://localhost:%d'.\n",
	            control_port);

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = receive_signal;
	// initialize the mask to be empty
	int empty_set_result = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);
	int result_act = sigaction(SIGINT, &action, NULL);
	if(result_act < 0 || empty_set_result < 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't set signal interception: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	// create pool and queue! then initializing both!
	// the pool is created and destroyed outside of the listener, so the listener can be
	// cancelled and then the main thread destroys everything accordingly
	ThreadPool control_pool;
	int create_result1 = pool_create_dynamic(&control_pool);
	if(create_result1 < 0) {
		print_create_error(-create_result1);
		return EXIT_FAILURE;
	}

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	Myqueue control_job_ids;
	if(myqueue_init(&control_job_ids) < 0) {
		return EXIT_FAILURE;
	};

	// this is an array of pointers
	ZVEC_TYPENAME(ConnectionContextPtr) control_contexts = ZVEC_EMPTY(ConnectionContextPtr);

	const ZvecResult allocate_result =
	    ZVEC_ALLOCATE_UNINITIALIZED_EXTENDED(ConnectionContext*, ConnectionContextPtr,
	                                         &control_contexts, control_pool.worker_threads_amount);

	if(allocate_result == ZvecResultErr) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	// TODO(Totto): implement implicit TLS
	const SecureOptions* const final_options =
	    is_secure(options) // NOLINT(readability-implicit-bool-conversion)
	        ? initialize_secure_options(false, "", "")
	        : options;

	for(size_t i = 0; i < control_pool.worker_threads_amount; ++i) {
		ConnectionContext* context = get_connection_context(final_options);

		auto _ = ZVEC_SET_AT_EXTENDED(ConnectionContext*, ConnectionContextPtr, &control_contexts,
		                              i, context);
		UNUSED(_);
	}

	size_t port_amount = DEFAULT_PASSIVE_PORT_AMOUNT;

	DataController* data_controller = initialize_data_controller(port_amount);

	if(!data_controller) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");

		for(size_t i = 0; i < control_pool.worker_threads_amount; ++i) {
			free_connection_context(ZVEC_AT(ConnectionContextPtr, control_contexts, i));
		}
		ZVEC_FREE(ConnectionContextPtr, &control_contexts);
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
	pthread_t data_orchestrator_thread = {};
	FTPDataOrchestratorArgument data_thread_argument = { .data_controller = data_controller,
		                                                 .port_amount = port_amount,
		                                                 .ports = ports };

	// creating the data thread
	int result2 = pthread_create(&data_orchestrator_thread, NULL,
	                             ftp_data_orchestrator_thread_function, &data_thread_argument);
	CHECK_FOR_THREAD_ERROR(result2,
	                       "An Error occurred while trying to create a new data listener Thread",
	                       return EXIT_FAILURE;);

	pthread_t control_listener_thread = {};
	FTPControlThreadArgument control_thread_argument = {
		.pool = &control_pool,
		.job_ids = &control_job_ids,
		.contexts = control_contexts,
		.socket_fd = control_socket_fd,
		.global_folder = folder,
		.data_controller = data_controller,
		.data_orchestrator = data_orchestrator_thread,
		.auth_providers = auth_providers,
	};

	// creating the control thread
	result1 = pthread_create(&control_listener_thread, NULL, ftp_control_listener_thread_function,
	                         &control_thread_argument);
	CHECK_FOR_THREAD_ERROR(result1,
	                       "An Error occurred while trying to create a new control listener Thread",
	                       return EXIT_FAILURE;);

	// wait for the single listener thread to finish, that happens when he is cancelled via
	// shutdown request
	ListenerError control_return_value = LISTENER_ERROR_NONE;
	result1 = pthread_join(control_listener_thread, &control_return_value);
	CHECK_FOR_THREAD_ERROR(result1, "An Error occurred while trying to wait for a control Thread",
	                       return EXIT_FAILURE;);

	if(is_listener_error(control_return_value)) {
		if(control_return_value != LISTENER_ERROR_NONE) {
			print_listener_error(control_return_value);
		}
	} else if(control_return_value != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "The control listener thread wasn't cancelled properly!\n");
	} else if(control_return_value == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The control listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError,
		            "The control listener thread was terminated with wrong error: %p!\n",
		            control_return_value);
	}

	ListenerError data_return_value = LISTENER_ERROR_NONE;
	result2 = pthread_join(data_orchestrator_thread, &data_return_value);
	CHECK_FOR_THREAD_ERROR(result2, "An Error occurred while trying to wait for a data Thread",
	                       return EXIT_FAILURE;);

	if(is_listener_error(data_return_value)) {
		if(data_return_value != LISTENER_ERROR_NONE) {
			print_listener_error(data_return_value);
		}
	} else if(data_return_value != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError,
		                   "The ftp data listener thread wasn't cancelled properly!\n");
	} else if(data_return_value == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The ftp data listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError,
		            "The ftp data listener thread was terminated with wrong error: %p!\n",
		            data_return_value);
	}

	// since the listener doesn't wait on the jobs, the main thread has to do that work!
	// the queue can be filled, which can lead to a problem!!
	while(!myqueue_is_empty(&control_job_ids)) {
		JobId* job_id = (JobId*)myqueue_pop(&control_job_ids);

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
	if(pool_destroy(&control_pool) < 0) {
		return EXIT_FAILURE;
	}

	// then the queue is destroyed
	if(myqueue_destroy(&control_job_ids) < 0) {
		return EXIT_FAILURE;
	}

	// finally closing the whole socket, so that the port is useable by other programs or by
	// this again, NOTES: ip(7) states :" A TCP local socket address that has been bound is
	// unavailable for  some time after closing, unless the SO_REUSEADDR flag has been set.
	// Care should be taken when using this flag as it makes TCP less reliable." So
	// essentially saying, also correctly closed sockets aren't available after a certain
	// time, even if closed correctly!
	result1 = close(control_socket_fd);
	CHECK_FOR_ERROR(result1, "While trying to close the control socket", return EXIT_FAILURE;);

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(control_addr);

	free(ports);

	for(size_t i = 0; i < control_pool.worker_threads_amount; ++i) {
		free_connection_context(ZVEC_AT(ConnectionContextPtr, control_contexts, i));
	}

	free(folder);

	free_authentication_providers(auth_providers);

#ifdef _SIMPLE_SERVER_USE_OPENSSL
	openssl_cleanup_global_state();
#endif

	return EXIT_SUCCESS;
}
