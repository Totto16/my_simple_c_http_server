

#include "./helper.h"
#include <signal.h>

bool setup_sigpipe_signal_handler(void) {

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = SIG_IGN;
	// initialize the mask to be empty
	int emptySetResult = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGPIPE);
	int result1 = sigaction(SIGPIPE, &action, NULL);
	if(result1 < 0 || emptySetResult < 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't set signal interception: %s\n", strerror(errno));
		return false;
	}

	return true;
}
