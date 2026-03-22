#include "ftp/server.h"
#include "generic/authentication.h"
#include "generic/secure.h"
#include "http/server.h"
#include "utils/log.h"
#include "utils/path.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define IDENT1 "\t"
#define IDENT2 IDENT1 IDENT1

static void print_http_server_usage(const bool is_subcommand) {
	if(is_subcommand) {
		printf("<port> [options]\n");
	} else {
		printf(IDENT1 "http <port> [options]\n");
	}

	printf(IDENT1 "port: the port to bind to (required)\n");
	printf(IDENT1 "options:\n");
	printf(IDENT2 "-s, --secure <public_cert_file> <private_cert_file>: Use a secure connection "
	              "(https), you have to provide the public and private certificates\n");
	printf(IDENT2 "-r, --route <route_name>: Use a certain route mapping\n");
	printf(IDENT2 "-l, --loglevel <loglevel>: Set the log level for the application\n");
}

static void print_ftp_server_usage(const bool is_subcommand) {
	if(is_subcommand) {
		printf("<port> [options]\n");
	} else {
		printf(IDENT1 "ftp <port> [options]\n");
	}

	printf(IDENT1 "port: the port to bind to (required)\n");
	printf(IDENT1 "options:\n");
	printf(IDENT2
	       "-s, --secure <public_cert_file> <private_cert_file>: Provide a secure variant of ftp "
	       "(ftps), you have to provide the public and private certificates. This uses implicit "
	       "TLS, and can negotiate TLS with clients, if needed\n");
	printf(IDENT2 "-f, --folder <folder>: The folder to serve, default is '.'\n");
	printf(IDENT2 "-l, --loglevel <loglevel>: Set the log level for the application\n");
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	UsageCommandAll = 0,
	UsageCommandHttp,
	UsageCommandFtp,
} UsageCommand;

// prints the usage, if argc is not the right amount!
static void print_usage(const tstr_static program_name, const UsageCommand usage_command) {
	switch(usage_command) {
		case UsageCommandHttp: {
			printf("usage: " TSTR_FMT " http ", TSTR_STATIC_FMT_ARGS(program_name));
			print_http_server_usage(true);
			break;
		}
		case UsageCommandFtp: {
			printf("usage: " TSTR_FMT " ftp ", TSTR_STATIC_FMT_ARGS(program_name));
			print_ftp_server_usage(true);
			break;
		}
		case UsageCommandAll:
		default: {
			printf("usage: " TSTR_FMT " <command>\n", TSTR_STATIC_FMT_ARGS(program_name));
			printf("commands: http, ftp\n");
			print_http_server_usage(false);
			print_ftp_server_usage(false);
			break;
		}
	}
}

NODISCARD static bool is_help_string(const tstr_static str) {
	if(tstr_static_eq(str, TSTR_STATIC_LIT("--help"))) {
		return true;
	}

	if(tstr_static_eq(str, TSTR_STATIC_LIT("-h"))) {
		return true;
	}

	if(tstr_static_eq(str, TSTR_STATIC_LIT("-?"))) {
		return true;
	}

	return false;
}

NODISCARD static bool is_version_string(const tstr_static str) {
	if(tstr_static_eq(str, TSTR_STATIC_LIT("--version"))) {
		return true;
	}

	if(tstr_static_eq(str, TSTR_STATIC_LIT("-v"))) {
		return true;
	}

	return false;
}

typedef struct {
	tstr username;
	tstr password;
	UserRole role;
} SimpleUserEntry;

NODISCARD static AuthenticationProviders* initialize_default_authentication_providers(void) {

	AuthenticationProviders* const auth_providers = initialize_authentication_providers();

	if(!auth_providers) {
		return NULL;
	}

	AuthenticationProvider* const simple_auth_provider =
	    initialize_simple_authentication_provider();

	if(!simple_auth_provider) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn, "Failed to initialize the simple auth provider\n")
	} else {

		const SimpleUserEntry entries[] = {
			{ .username = TSTR_LIT("admin"), .password = TSTR_LIT("admin"), .role = UserRoleAdmin }
		};

		for(size_t i = 0; i < sizeof(entries) / sizeof(*entries); ++i) {

			const SimpleUserEntry entry = entries[i];

			const bool result = add_user_to_simple_authentication_provider_data_password_raw(
			    simple_auth_provider, &entry.username, &entry.password, entry.role);

			if(!result) {
				free_authentication_providers(auth_providers);
				free_authentication_provider(simple_auth_provider);
				return NULL;
			}
		}

		if(!add_authentication_provider(auth_providers, simple_auth_provider)) {
			free_authentication_providers(auth_providers);
			return NULL;
		}
	}

	AuthenticationProvider* const system_auth_provider =
	    initialize_system_authentication_provider();

	if(!system_auth_provider) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn, "Failed to initialize the simple auth provider\n")
	} else {
		if(!add_authentication_provider(auth_providers, system_auth_provider)) {
			free_authentication_providers(auth_providers);
			return NULL;
		}
	}

	return auth_providers;
}

typedef struct {
	size_t size;
	const char* const* data;
} ProgramArgs;

#define PROGRAM_ARGS_AT(args, index) \
	(assert((index) < (args).size), tstr_static_from_static_cstr((args).data[(index)]))

static inline ProgramArgs advance_program_args(const ProgramArgs args, const size_t amount) {
	assert(args.size >= amount);
	return (ProgramArgs){ .size = args.size - amount, .data = args.data + amount };
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	RouteIdentifierDefault = 0,
	RouteIdentifierTestSuiteWebserverTester,
} RouteIdentifier;

NODISCARD static int subcommand_http(const tstr_static program_name, const ProgramArgs args) {

	if(args.size < 1) {
		fprintf(stderr, "missing <port>\n");
		print_usage(program_name, UsageCommandHttp);
		return EXIT_FAILURE;
	}

	const tstr_static arg0 = PROGRAM_ARGS_AT(args, 0);

	if(is_help_string(arg0)) {
		printf("'http' command help menu:\n");
		print_usage(program_name, UsageCommandHttp);
		return EXIT_SUCCESS;
	}

	// parse the port
	// TODO(Totto): don't use the .ptr here
	const uint16_t port = parse_u16_safely(arg0.ptr, "<port>");

	bool secure = false;
	tstr_static public_cert_file = tstr_static_null();
	tstr_static private_cert_file = tstr_static_null();

	RouteIdentifier route_identifier = RouteIdentifierDefault;

	LogLevel log_level =
#ifdef NDEBUG
	    LogLevelError
#else
	    LogLevelTrace
#endif
	    ;

	// the port
	size_t processed_args = 1;

	while(processed_args != args.size) {

		const tstr_static arg = PROGRAM_ARGS_AT(args, processed_args);

		if(tstr_static_eq(arg, TSTR_STATIC_LIT("-s")) ||
		   tstr_static_eq(arg, TSTR_STATIC_LIT("--secure"))) {
#ifdef _SIMPLE_SERVER_SECURE_DISABLED
			fprintf(stderr, "Server was build without support for 'secure'\n");
			print_usage(program_name, UsageCommandHttp);
			return EXIT_FAILURE;
#else
			secure = true;
			if(processed_args + 3 > args.size) {
				fprintf(stderr, "Not enough arguments for the 'secure' option\n");
				print_usage(program_name, UsageCommandHttp);
				return EXIT_FAILURE;
			}

			public_cert_file = PROGRAM_ARGS_AT(args, processed_args + 1);
			private_cert_file = PROGRAM_ARGS_AT(args, processed_args + 2);
			processed_args += 3;
#endif
		} else if(tstr_static_eq(arg, TSTR_STATIC_LIT("-l")) ||
		          tstr_static_eq(arg, TSTR_STATIC_LIT("--loglevel"))) {
			if(processed_args + 2 > args.size) {
				fprintf(stderr, "Not enough arguments for the 'loglevel' option\n");
				print_usage(program_name, UsageCommandHttp);
				return EXIT_FAILURE;
			}

			const tstr_static loglevel_arg = PROGRAM_ARGS_AT(args, processed_args + 1);

			bool success = false;
			const LogLevel parsed_level = parse_log_level(loglevel_arg, &success);

			if(!success) {
				fprintf(stderr,
				        "Wrong option for the 'loglevel' option, unrecognized level: " TSTR_FMT
				        "\n",
				        TSTR_STATIC_FMT_ARGS(loglevel_arg));
				print_usage(program_name, UsageCommandHttp);
				return EXIT_FAILURE;
			}

			log_level = parsed_level;

			processed_args += 2;
		} else if(tstr_static_eq(arg, TSTR_STATIC_LIT("-r")) ||
		          tstr_static_eq(arg, TSTR_STATIC_LIT("--route"))) {
			if(processed_args + 2 > args.size) {
				fprintf(stderr, "Not enough arguments for the 'route' option\n");
				print_usage(program_name, UsageCommandHttp);
				return EXIT_FAILURE;
			}

			const tstr_static route_name = PROGRAM_ARGS_AT(args, processed_args + 1);

			if(tstr_static_eq(route_name, TSTR_STATIC_LIT("default"))) {
				route_identifier = RouteIdentifierDefault;
			} else if(tstr_static_eq(route_name, TSTR_STATIC_LIT("webserver_tester"))) {
				route_identifier = RouteIdentifierTestSuiteWebserverTester;
			} else {
				fprintf(stderr,
				        "Wrong option for the 'route' option, unrecognized route name: " TSTR_FMT
				        "\n",
				        TSTR_STATIC_FMT_ARGS(route_name));
				print_usage(program_name, UsageCommandHttp);
				return EXIT_FAILURE;
			}

			processed_args += 2;
		} else {
			fprintf(stderr, "Unrecognized option: " TSTR_FMT "\n", TSTR_STATIC_FMT_ARGS(arg));
			print_usage(program_name, UsageCommandHttp);
			return EXIT_FAILURE;
		}
	}

	initialize_logger();

	set_log_level(log_level);

	set_thread_name("main thread");

	LOG_MESSAGE(LogLevelTrace, "Setting LogLevel to %s\n", get_level_name(log_level));
	const tstr_static secure_string =
	    secure ? TSTR_STATIC_LIT("true") // NOLINT(readability-implicit-bool-conversion)
	           : TSTR_STATIC_LIT("false");
	LOG_MESSAGE(LogLevelTrace, "Using secure connections: " TSTR_FMT "\n",
	            TSTR_STATIC_FMT_ARGS(secure_string));

	SecureOptions* const options =
	    initialize_secure_options(secure, public_cert_file, private_cert_file);

	if(options == NULL) {
		fprintf(stderr, "Couldn't initialize secure options\n");
		return EXIT_FAILURE;
	}

	AuthenticationProviders* const auth_providers = initialize_default_authentication_providers();

	if(auth_providers == NULL) {
		fprintf(stderr, "Couldn't initialize authentication providers\n");
		free_secure_options(options);
		return EXIT_FAILURE;
	}

	HTTPRoutes* routes = NULL;

	switch(route_identifier) {
		case RouteIdentifierDefault: {
			routes = get_default_routes();
			break;
		}
		case RouteIdentifierTestSuiteWebserverTester: {
			routes = get_webserver_test_routes();
			break;
		}
		default: {
			routes = NULL;
			break;
		}
	}

	if(routes == NULL) {
		fprintf(stderr, "Couldn't initialize routes\n");
		return EXIT_FAILURE;
	}

	return start_http_server(port, MOVE(options), MOVE(auth_providers), MOVE(routes));
}

NODISCARD static int subcommand_ftp(const tstr_static program_name, const ProgramArgs args) {

	if(args.size < 1) {
		fprintf(stderr, "missing <port>\n");
		print_usage(program_name, UsageCommandFtp);
		return EXIT_FAILURE;
	}

	const tstr_static arg0 = PROGRAM_ARGS_AT(args, 0);

	if(is_help_string(arg0)) {
		printf("'ftp' command help menu:\n");
		print_usage(program_name, UsageCommandFtp);
		return EXIT_SUCCESS;
	}

	// TODO(Totto): don't use the .ptr here
	const uint16_t control_port = parse_u16_safely(arg0.ptr, "<port>");

	bool secure = false;
	tstr_static public_cert_file = tstr_static_null();
	tstr_static private_cert_file = tstr_static_null();

	LogLevel log_level =
#ifdef NDEBUG
	    LogLevelError
#else
	    LogLevelTrace
#endif
	    ;

	tstr_static folder_to_resolve = TSTR_STATIC_LIT(".");

	// the port
	size_t processed_args = 1;

	while(processed_args != args.size) {

		const tstr_static arg = PROGRAM_ARGS_AT(args, processed_args);

		if(tstr_static_eq(arg, TSTR_STATIC_LIT("-s")) ||
		   tstr_static_eq(arg, TSTR_STATIC_LIT("--secure"))) {
#ifdef _SIMPLE_SERVER_SECURE_DISABLED
			fprintf(stderr, "Server was build without support for 'secure'\n");
			print_usage(program_name, UsageCommandFtp);
			return EXIT_FAILURE;
#else
			secure = true;
			if(processed_args + 3 > args.size) {
				fprintf(stderr, "Not enough arguments for the 'secure' option\n");
				print_usage(program_name, UsageCommandHttp);
				return EXIT_FAILURE;
			}

			public_cert_file = PROGRAM_ARGS_AT(args, processed_args + 1);
			private_cert_file = PROGRAM_ARGS_AT(args, processed_args + 2);
			processed_args += 3;
#endif
		} else if(tstr_static_eq(arg, TSTR_STATIC_LIT("-f")) ||
		          tstr_static_eq(arg, TSTR_STATIC_LIT("--folder"))) {
			if(processed_args + 2 > args.size) {
				fprintf(stderr, "Not enough arguments for the 'folder' option\n");
				print_usage(program_name, UsageCommandFtp);
				return EXIT_FAILURE;
			}

			folder_to_resolve = PROGRAM_ARGS_AT(args, processed_args + 1);
			processed_args += 2;

		} else if(tstr_static_eq(arg, TSTR_STATIC_LIT("-l")) ||
		          tstr_static_eq(arg, TSTR_STATIC_LIT("--loglevel"))) {
			if(processed_args + 2 > args.size) {
				fprintf(stderr, "Not enough arguments for the 'loglevel' option\n");
				print_usage(program_name, UsageCommandFtp);
				return EXIT_FAILURE;
			}

			const tstr_static loglevel_arg = PROGRAM_ARGS_AT(args, processed_args + 1);

			bool success = false;
			const LogLevel parsed_level = parse_log_level(loglevel_arg, &success);

			if(!success) {
				fprintf(stderr,
				        "Wrong option for the 'loglevel' option, unrecognized level: " TSTR_FMT
				        "\n",
				        TSTR_STATIC_FMT_ARGS(loglevel_arg));
				print_usage(program_name, UsageCommandFtp);
				return EXIT_FAILURE;
			}

			log_level = parsed_level;

			processed_args += 2;
		} else {
			fprintf(stderr, "Unrecognized option: " TSTR_FMT "\n", TSTR_STATIC_FMT_ARGS(arg));
			print_usage(program_name, UsageCommandFtp);
			return EXIT_FAILURE;
		}
	}

	const tstr folder_to_resolve_temp = tstr_from_static_tstr(folder_to_resolve);

	const tstr folder = get_serve_folder(&folder_to_resolve_temp);

	if(tstr_is_null(&folder)) {
		return EXIT_FAILURE;
	}

	initialize_logger();

	set_log_level(log_level);

	set_thread_name("main thread");

	LOG_MESSAGE(LogLevelTrace, "Setting LogLevel to %s\n", get_level_name(log_level));

	const tstr_static secure_string =
	    secure ? TSTR_STATIC_LIT("true") // NOLINT(readability-implicit-bool-conversion)
	           : TSTR_STATIC_LIT("false");
	LOG_MESSAGE(LogLevelTrace, "Providing implicit TLS: " TSTR_FMT "\n",
	            TSTR_STATIC_FMT_ARGS(secure_string));

	SecureOptions* const options =
	    initialize_secure_options(secure, public_cert_file, private_cert_file);

	if(options == NULL) {
		fprintf(stderr, "Couldn't initialize secure options\n");
		return EXIT_FAILURE;
	}

	AuthenticationProviders* const auth_providers = initialize_default_authentication_providers();

	if(auth_providers == NULL) {
		fprintf(stderr, "Couldn't initialize authentication providers\n");
		return EXIT_FAILURE;
	}

	return start_ftp_server(control_port, folder, options, auth_providers);
}

static int rich_main(const ProgramArgs args) {
	if(args.size < 1) {
		fprintf(stderr, "No program name specified: FATAL ERROR\n");
		return EXIT_FAILURE;
	}

	const tstr_static program_name = PROGRAM_ARGS_AT(args, 0);

	// checking if there are enough arguments
	if(args.size < 2) {
		fprintf(stderr, "No command specified\n");
		print_usage(program_name, UsageCommandAll);
		return EXIT_FAILURE;
	}

	const tstr_static command = PROGRAM_ARGS_AT(args, 1);

	if(tstr_static_eq(command, TSTR_STATIC_LIT("http"))) {
		return subcommand_http(program_name, advance_program_args(args, 2));
	}

	if(tstr_static_eq(command, TSTR_STATIC_LIT("ftp"))) {
		return subcommand_ftp(program_name, advance_program_args(args, 2));
	}

	if(is_help_string(command)) {
		printf("General help menu:\n");
		print_usage(program_name, UsageCommandAll);
		return EXIT_SUCCESS;
	}

	if(is_version_string(command)) {
		printf(STRINGIFY(VERSION_STRING) "\n");
		return EXIT_SUCCESS;
	}

	fprintf(stderr, "Invalid command '" TSTR_FMT "'\n", TSTR_STATIC_FMT_ARGS(command));
	print_usage(program_name, UsageCommandAll);
	return EXIT_FAILURE;
}

int main(const int argc, const char* const* const argv) {
	ProgramArgs args = { .size = argc, .data = argv };
	return rich_main(args);
}
