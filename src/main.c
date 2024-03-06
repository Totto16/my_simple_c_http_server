/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 08
*/

#include "server.h"

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName) {
	printf("usage: %s <port>\n", programName);
}

int main(int argc, char const* argv[]) {

	// checking if there are enough arguments
	if(argc != 2) {
		printUsage(argv[0]);
		return EXIT_FAILURE;
	}

	// parse the port
	long port = parseLongSafely(argv[1], "<port>");

	return startServer(port);
}
