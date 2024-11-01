/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include "generic/secure.h"
#include "server.h"

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName) {
	printf("usage: %s <port> [options]\n", programName);
	printf("options:\n");
	printf("\t-s, --secure <public_cert_file> <private_cert_file>: Use a secure connection "
	       "(https), you have to provide the public and private certificates\n");
}

int main(int argc, const char* argv[]) {

	// checking if there are enough arguments
	if(argc != 2 && argc != 5) {
		printUsage(argv[0]);
		return EXIT_FAILURE;
	}

	// parse the port
	uint16_t port = parseU16Safely(argv[1], "<port>");

	bool secure = false;
	const char* public_cert_file = "";
	const char* private_cert_file = "";

	if(argc == 5) {
		if(strcmp(argv[2], "-s") == 0) {
			secure = true;
			public_cert_file = argv[3];
			private_cert_file = argv[4];
		} else if(strcmp(argv[2], "--secure") == 0) {
			secure = true;
			public_cert_file = argv[3];
			private_cert_file = argv[4];
		} else {
			printUsage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	SecureOptions* options = initialize_secure_options(secure, public_cert_file, private_cert_file);

	if(options == NULL) {
		fprintf(stderr, "Couldn't initialize secure options\n");
		exit(1);
	}

	return startServer(port, options);
}

// TODO: general, don't use exit() in error cases, try to do the best to "recover" in such
// situations
