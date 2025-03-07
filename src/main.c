/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include "generic/secure.h"
#include "server.h"
#include "utils/log.h"

#define IDENT1 "\t"
#define IDENT2 IDENT1 IDENT1

void printHttpServerUsage() {
	printf(IDENT1 "http <port> [options]\n");
	printf(IDENT1 "port: the port to bind to (required)");
	printf(IDENT1 "options:\n");
	printf(IDENT2 "-s, --secure <public_cert_file> <private_cert_file>: Use a secure connection "
	              "(https), you have to provide the public and private certificates\n");
	printf(IDENT2 "-l, --loglevel <loglevel>: Set the log level for the application\n");
}

void printFtpServerUsage() {
	printf(IDENT1 "ftp <port> [folder] [options]\n");
	printf(IDENT1 "port: the port to bind to (required)");
	printf(IDENT1 "folder: the folder to server (optional) default: '.'");
	printf(IDENT1 "options:\n");
	printf(IDENT2 "-l, --loglevel <loglevel>: Set the log level for the application\n");
}

typedef enum { USAGE_COMMAND_ALL = 0, USAGE_COMMAND_HTTP = 1, USAGE_COMMAND_FTP = 2 } USAGE_COMMAND;

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName, USAGE_COMMAND usage_command) {
	switch(usage_command) {
		case USAGE_COMMAND_HTTP: {
			printf("usage: %s http", programName);
			printHttpServerUsage();
			break;
		}

		case USAGE_COMMAND_FTP: {
			printf("usage: %s ftp", programName);
			printFtpServerUsage();
			break;
		}
		case USAGE_COMMAND_ALL:
		default: {
			printf("usage: %s <command>", programName);
			printf("commands: http, ftp\n");
			printHttpServerUsage();
			printFtpServerUsage();
			break;
		}
	}
}

int subcommandHttp(const char* programName, int argc, const char* argv[]) {

	if(argc < 1) {
		printUsage(programName, USAGE_COMMAND_HTTP);
		return EXIT_FAILURE;
	}

	// parse the port
	uint16_t port = parseU16Safely(argv[0], "<port>");

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
#ifdef _HTTP_SERVER_SECURE_DISABLED
			fprintf(stderr, "Server was build without support for 'secure'\n");
			printUsage(argv[0]);
			return EXIT_FAILURE;
#else
			secure = true;
			if(processed_args + 3 > argc) {
				fprintf(stderr, "Not enough arguments for the 'secure' option\n");
				printUsage(argv[0], USAGE_COMMAND_HTTP);
				return EXIT_FAILURE;
			}

			public_cert_file = argv[processed_args + 1];
			private_cert_file = argv[processed_args + 2];
			processed_args += 3;
#endif
		} else if((strcmp(arg, "-l") == 0) || (strcmp(arg, "--loglevel") == 0)) {
			if(processed_args + 2 > argc) {
				fprintf(stderr, "Not enough arguments for the 'loglevel' option\n");
				printUsage(argv[0], USAGE_COMMAND_HTTP);
				return EXIT_FAILURE;
			}

			int parsed_level = parse_log_level(argv[processed_args + 1]);

			if(parsed_level < 0) {
				fprintf(stderr, "Wrong option for the 'loglevel' option, unrecognized level: %s\n",
				        arg);
				printUsage(argv[0], USAGE_COMMAND_HTTP);
				return EXIT_FAILURE;
			}

			log_level = parsed_level;

			processed_args += 2;
		} else {
			fprintf(stderr, "Unrecognized option: %s\n", arg);
			printUsage(argv[0], USAGE_COMMAND_HTTP);
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

	return startServer(port, options);
}

int subcommandFtp(const char* programName, int argc, const char* argv[]) {

	UNUSED(programName);
	UNUSED(argc);
	UNUSED(argv);
	return EXIT_FAILURE;
}

int main(int argc, const char* argv[]) {

	// checking if there are enough arguments
	if(argc < 2) {
		printUsage(argv[0], USAGE_COMMAND_ALL);
		return EXIT_FAILURE;
	}

	const char* command = argv[1];

	if(strcmp(command, "http") == 0) {
		return subcommandHttp(argv[0], argc - 2, argv + 2);
	} else if(strcmp(command, "ftp") == 0) {
		return subcommandFtp(argv[0], argc - 2, argv + 2);
	} else {
		printUsage(argv[0], USAGE_COMMAND_ALL);
		return EXIT_FAILURE;
	}
}
