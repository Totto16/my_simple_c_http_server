#include "ftp/server.h"
#include "generic/authentication.h"
#include "generic/secure.h"
#include "http/server.h"
#include "utils/log.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define IDENT1 "\t"
#define IDENT2 IDENT1 IDENT1

static void print_http_server_usage(bool is_subcommand) {
	if(is_subcommand) {
		printf("<port> [options]\n");
	} else {
		printf(IDENT1 "http <port> [options]\n");
	}

	printf(IDENT1 "port: the port to bind to (required)\n");
	printf(IDENT1 "options:\n");
	printf(IDENT2 "-s, --secure <public_cert_file> <private_cert_file>: Use a secure connection "
	              "(https), you have to provide the public and private certificates\n");
	printf(IDENT2 "-l, --loglevel <loglevel>: Set the log level for the application\n");
}

static void print_ftp_server_usage(bool is_subcommand) {
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
static void print_usage(const char* program_name, UsageCommand usage_command) {
	switch(usage_command) {
		case UsageCommandHttp: {
			printf("usage: %s http ", program_name);
			print_http_server_usage(true);
			break;
		}

		case UsageCommandFtp: {
			printf("usage: %s ftp ", program_name);
			print_ftp_server_usage(true);
			break;
		}
		case UsageCommandAll:
		default: {
			printf("usage: %s <command>\n", program_name);
			printf("commands: http, ftp\n");
			print_http_server_usage(false);
			print_ftp_server_usage(false);
			break;
		}
	}
}

NODISCARD static bool is_help_string(const char* str) {
	if(strcmp(str, "--help") == 0) {
		return true;
	}

	if(strcmp(str, "-h") == 0) {
		return true;
	}

	if(strcmp(str, "-?") == 0) {
		return true;
	}

	return false;
}

typedef struct {
	char* username;
	char* password;
	char* role;
} SimpleUserEntry;

NODISCARD static AuthenticationProviders* initialize_default_authentication_providers(void) {

	AuthenticationProviders* auth_providers = initialize_authentication_providers();

	if(!auth_providers) {
		return NULL;
	}

	AuthenticationProvider* simple_auth_provider = initialize_simple_authentication_provider();

	if(!simple_auth_provider) {
		free_authentication_providers(auth_providers);
		return NULL;
	}

	SimpleUserEntry entries[] = { { .username = "admin", .password = "admin", .role = "admin" } };

	for(size_t i = 0; i < sizeof(entries) / sizeof(*entries); ++i) {

		SimpleUserEntry entry = entries[i];

		if(!add_user_to_simple_authentication_provider_data_password_raw(
		       simple_auth_provider, entry.username, entry.password, entry.role)) {
			free_authentication_providers(auth_providers);
			return NULL;
		}
	}

	if(!add_authentication_provider(auth_providers, simple_auth_provider)) {
		free_authentication_providers(auth_providers);
		return NULL;
	}

	AuthenticationProvider* system_auth_provider = initialize_system_authentication_provider();

	if(!system_auth_provider) {
		free_authentication_providers(auth_providers);
		return NULL;
	}

	if(!add_authentication_provider(auth_providers, system_auth_provider)) {
		free_authentication_providers(auth_providers);
		return NULL;
	}

	return auth_providers;
}

NODISCARD static int subcommand_http(const char* program_name, int argc, const char* argv[]) {

	if(argc < 1) {
		fprintf(stderr, "missing <port>\n");
		print_usage(program_name, UsageCommandHttp);
		return EXIT_FAILURE;
	}

	if(is_help_string(argv[0])) {
		printf("'http' command help menu:\n");
		print_usage(program_name, UsageCommandHttp);
		return EXIT_SUCCESS;
	}

	// parse the port
	uint16_t port = parse_u16_safely(argv[0], "<port>");

	bool secure = false;
	const char* public_cert_file = "";
	const char* private_cert_file = "";

	LogLevel log_level =
#ifdef NDEBUG
	    LogLevelError
#else
	    LogLevelTrace
#endif
	    ;

	// the port
	int processed_args = 1;

	while(processed_args != argc) {

		const char* arg = argv[processed_args];

		if((strcmp(arg, "-s") == 0) || (strcmp(arg, "--secure") == 0)) {
#ifdef _SIMPLE_SERVER_SECURE_DISABLED
			fprintf(stderr, "Server was build without support for 'secure'\n");
			print_usage(argv[0], UsageCommandHttp);
			return EXIT_FAILURE;
#else
			secure = true;
			if(processed_args + 3 > argc) {
				fprintf(stderr, "Not enough arguments for the 'secure' option\n");
				print_usage(argv[0], UsageCommandHttp);
				return EXIT_FAILURE;
			}

			public_cert_file = argv[processed_args + 1];
			private_cert_file = argv[processed_args + 2];
			processed_args += 3;
#endif
		} else if((strcmp(arg, "-l") == 0) || (strcmp(arg, "--loglevel") == 0)) {
			if(processed_args + 2 > argc) {
				fprintf(stderr, "Not enough arguments for the 'loglevel' option\n");
				print_usage(argv[0], UsageCommandHttp);
				return EXIT_FAILURE;
			}

			int parsed_level = parse_log_level(argv[processed_args + 1]);

			if(parsed_level < 0) {
				fprintf(stderr, "Wrong option for the 'loglevel' option, unrecognized level: %s\n",
				        argv[processed_args + 1]);
				print_usage(argv[0], UsageCommandHttp);
				return EXIT_FAILURE;
			}

			log_level = parsed_level;

			processed_args += 2;
		} else {
			fprintf(stderr, "Unrecognized option: %s\n", arg);
			print_usage(argv[0], UsageCommandHttp);
			return EXIT_FAILURE;
		}
	}

	initialize_logger();

	set_log_level(log_level);

	set_thread_name("main thread");

	LOG_MESSAGE(LogLevelTrace, "Setting LogLevel to %s\n", get_level_name(log_level));
	const char* secure_string =
	    secure ? "true" : "false"; // NOLINT(readability-implicit-bool-conversion)
	LOG_MESSAGE(LogLevelTrace, "Using secure connections: %s\n", secure_string);

	SecureOptions* options = initialize_secure_options(secure, public_cert_file, private_cert_file);

	if(options == NULL) {
		fprintf(stderr, "Couldn't initialize secure options\n");
		return EXIT_FAILURE;
	}

	AuthenticationProviders* auth_providers = initialize_default_authentication_providers();

	if(auth_providers == NULL) {
		fprintf(stderr, "Couldn't initialize authentication providers\n");
		return EXIT_FAILURE;
	}

	return start_http_server(port, options, auth_providers);
}

NODISCARD static int subcommand_ftp(const char* program_name, int argc, const char* argv[]) {

	if(argc < 1) {
		fprintf(stderr, "missing <port>\n");
		print_usage(program_name, UsageCommandFtp);
		return EXIT_FAILURE;
	}

	if(is_help_string(argv[0])) {
		printf("'ftp' command help menu:\n");
		print_usage(program_name, UsageCommandFtp);
		return EXIT_SUCCESS;
	}

	// parse the port
	uint16_t control_port = parse_u16_safely(argv[0], "<port>");

	bool secure = false;
	const char* public_cert_file = "";
	const char* private_cert_file = "";

	LogLevel log_level =
#ifdef NDEBUG
	    LogLevelError
#else
	    LogLevelTrace
#endif
	    ;

	const char* folder_to_resolve = ".";

	// the port
	int processed_args = 1;

	while(processed_args != argc) {

		const char* arg = argv[processed_args];

		if((strcmp(arg, "-s") == 0) || (strcmp(arg, "--secure") == 0)) {
#ifdef _SIMPLE_SERVER_SECURE_DISABLED
			fprintf(stderr, "Server was build without support for 'secure'\n");
			print_usage(argv[0], UsageCommandFtp);
			return EXIT_FAILURE;
#else
			secure = true;
			if(processed_args + 3 > argc) {
				fprintf(stderr, "Not enough arguments for the 'secure' option\n");
				print_usage(argv[0], UsageCommandHttp);
				return EXIT_FAILURE;
			}

			public_cert_file = argv[processed_args + 1];
			private_cert_file = argv[processed_args + 2];
			processed_args += 3;
#endif
		} else if((strcmp(arg, "-f") == 0) || (strcmp(arg, "--folder") == 0)) {
			if(processed_args + 2 > argc) {
				fprintf(stderr, "Not enough arguments for the 'folder' option\n");
				print_usage(argv[0], UsageCommandFtp);
				return EXIT_FAILURE;
			}

			folder_to_resolve = argv[processed_args + 1];
			processed_args += 2;

		} else if((strcmp(arg, "-l") == 0) || (strcmp(arg, "--loglevel") == 0)) {
			if(processed_args + 2 > argc) {
				fprintf(stderr, "Not enough arguments for the 'loglevel' option\n");
				print_usage(argv[0], UsageCommandFtp);
				return EXIT_FAILURE;
			}

			int parsed_level = parse_log_level(argv[processed_args + 1]);

			if(parsed_level < 0) {
				fprintf(stderr, "Wrong option for the 'loglevel' option, unrecognized level: %s\n",
				        argv[processed_args + 1]);
				print_usage(argv[0], UsageCommandFtp);
				return EXIT_FAILURE;
			}

			log_level = parsed_level;

			processed_args += 2;
		} else {
			fprintf(stderr, "Unrecognized option: %s\n", arg);
			print_usage(argv[0], UsageCommandFtp);
			return EXIT_FAILURE;
		}
	}

	char* folder = realpath(folder_to_resolve, NULL);

	if(folder == NULL) {
		fprintf(stderr, "Couldn't resolve folder '%s': %s\n", folder_to_resolve, strerror(errno));
		return EXIT_FAILURE;
	}

	struct stat stat_result;
	int result = stat(folder, &stat_result);

	if(result != 0) {
		fprintf(stderr, "Couldn't stat folder '%s': %s\n", folder, strerror(errno));
		return EXIT_FAILURE;
	}

	if(!(S_ISDIR(stat_result.st_mode))) {
		fprintf(stderr, "Folder '%s' is not a directory\n", folder);
		return EXIT_FAILURE;
	}

	if(access(folder, R_OK) != 0) {
		fprintf(stderr, "Can read from folder '%s': %s\n", folder, strerror(errno));
		return EXIT_FAILURE;
	}

	initialize_logger();

	set_log_level(log_level);

	set_thread_name("main thread");

	LOG_MESSAGE(LogLevelTrace, "Setting LogLevel to %s\n", get_level_name(log_level));

	const char* secure_string =
	    secure ? "true" : "false"; // NOLINT(readability-implicit-bool-conversion)
	LOG_MESSAGE(LogLevelTrace, "Providing implicit TLS: %s\n", secure_string);

	SecureOptions* options = initialize_secure_options(secure, public_cert_file, private_cert_file);

	if(options == NULL) {
		fprintf(stderr, "Couldn't initialize secure options\n");
		return EXIT_FAILURE;
	}

	AuthenticationProviders* auth_providers = initialize_default_authentication_providers();

	if(auth_providers == NULL) {
		fprintf(stderr, "Couldn't initialize authentication providers\n");
		return EXIT_FAILURE;
	}

	return start_ftp_server(control_port, folder, options, auth_providers);
}

int main(int argc, const char* argv[]) {

	// checking if there are enough arguments
	if(argc < 2) {
		fprintf(stderr, "No command specified\n");
		print_usage(argv[0], UsageCommandAll);
		return EXIT_FAILURE;
	}

	const char* command = argv[1];

	if(strcmp(command, "http") == 0) {
		return subcommand_http(argv[0], argc - 2, argv + 2);
	}

	if(strcmp(command, "ftp") == 0) {
		return subcommand_ftp(argv[0], argc - 2, argv + 2);
	}

	if(is_help_string(command)) {
		printf("General help menu:\n");
		print_usage(argv[0], UsageCommandAll);
		return EXIT_SUCCESS;
	}

	fprintf(stderr, "Invalid command '%s'\n", command);
	print_usage(argv[0], UsageCommandAll);
	return EXIT_FAILURE;
}
