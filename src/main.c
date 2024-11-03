/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include "generic/secure.h"
#include "server.h"
#include "utils/log.h"

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName) {
	printf("usage: %s <port> [options]\n", programName);
	printf("options:\n");
	printf("\t-s, --secure <public_cert_file> <private_cert_file>: Use a secure connection "
	       "(https), you have to provide the public and private certificates\n");
	printf("\t-l, --loglevel <loglevel>: Set the log level for the application\n");
}

int main(int argc, const char* argv[]) {

	// checking if there are enough arguments
	if(argc < 2) {
		printUsage(argv[0]);
		return EXIT_FAILURE;
	}

	// parse the port
	uint16_t port = parseU16Safely(argv[1], "<port>");

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

	// the port and the program name
	int processed_args = 2;

	while(processed_args != argc) {

		const char* arg = argv[processed_args];

		if((strcmp(arg, "-s") == 0) || (strcmp(arg, "--secure") == 0)) {
			secure = true;
			if(processed_args + 3 > argc) {
				fprintf(stderr, "Not enough arguments for the 'secure' option\n");
				printUsage(argv[0]);
				return EXIT_FAILURE;
			}

			public_cert_file = argv[processed_args + 1];
			private_cert_file = argv[processed_args + 2];
			processed_args += 3;
		} else if((strcmp(arg, "-l") == 0) || (strcmp(arg, "--loglevel") == 0)) {
			if(processed_args + 2 > argc) {
				fprintf(stderr, "Not enough arguments for the 'loglevel' option\n");
				printUsage(argv[0]);
				return EXIT_FAILURE;
			}

			int parsed_level = parse_log_level(argv[processed_args + 1]);

			if(parsed_level < 0) {
				fprintf(stderr, "Wrong option for the 'loglevel' option, unrecognized level: %s\n",
				        arg);
				printUsage(argv[0]);
				return EXIT_FAILURE;
			}

			processed_args += 2;
		} else {
			fprintf(stderr, "Unrecognized option: %s\n", arg);
			printUsage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	initialize_logger();

	set_log_level(log_level);

	set_thread_name("main thread");

	SecureOptions* options = initialize_secure_options(secure, public_cert_file, private_cert_file);

	if(options == NULL) {
		fprintf(stderr, "Couldn't initialize secure options\n");
		return EXIT_FAILURE;
	}

	return startServer(port, options);
}

// TODO: general, don't use exit() in error cases, try to do the best to "recover" in such
// situations
