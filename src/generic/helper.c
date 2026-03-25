

#include "./helper.h"
#include "utils/log.h"

#include <signal.h>

#ifdef _DONT_HAVE_SYS_SYSINFO
	#include <unistd.h>
#else
	#include <sys/sysinfo.h>
#endif

bool setup_sigpipe_signal_handler(void) {

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = SIG_IGN;
	// initialize the mask to be empty
	int empty_set_result = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGPIPE);
	int result1 = sigaction(SIGPIPE, &action, NULL);
	if(result1 < 0 || empty_set_result < 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't set signal interception: %s\n", strerror(errno));
		return false;
	}

	return true;
}

NODISCARD size_t get_active_cpu_cores(void) {
#ifdef _DONT_HAVE_SYS_SYSINFO
	// see https://www.unix.com/man_page/osx/3/sysconf
	return sysconf(_SC_NPROCESSORS_ONLN);
#else
	return get_nprocs();
#endif
}
