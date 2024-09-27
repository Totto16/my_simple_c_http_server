/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include "server.h"

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName) {
	printf("usage: %s <port> [options]\n", programName);
	printf("options:\n");
	printf("\t-s, --secure: USe a secure connection (https)\n");
}

int main(int argc, char const* argv[]) {

	// checking if there are enough arguments
	if(argc != 2 && argc != 3) {
		printUsage(argv[0]);
		return EXIT_FAILURE;
	}

	// parse the port
	uint16_t port = parseU16Safely(argv[1], "<port>");

	bool secure = false;

	if(argc == 3) {
		if(strcmp(argv[2], "-s") == 0) {
			secure = true;
		} else if(strcmp(argv[2], "--secure") == 0) {
			secure = true;
		} else {
			printUsage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	return startServer(port, secure);
}
