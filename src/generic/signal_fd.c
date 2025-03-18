

#ifdef _DONT_HAVE_SYS_SIGNALFD
#include <stdlib.h>
#include <sys/event.h>
#else
#include <signal.h>
#include <sys/signalfd.h>
#endif

#include "./signal_fd.h"

int get_signal_like_fd(int signal) {

#ifdef _DONT_HAVE_SYS_SIGNALFD
	// see https://stackoverflow.com/questions/10591531/alternative-to-signalfd
	// and https://gist.github.com/azat/73638b8e3b0fa563a20dadcca9e652a1
	// and https://man.freebsd.org/cgi/man.cgi?query=kqueue&sektion=2
	int kqueue_id = kqueue();

	// TODO(Totto): this war not yet tested
	struct kevent sigevent;
	EV_SET(&sigevent, signal, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, NULL);
	kevent(kq, &sigevent, 1, NULL, 0, NULL);
	return kqueue_id;
#else
	sigset_t mySigset;
	sigemptyset(&mySigset);
	sigaddset(&mySigset, signal);

	// see https://man7.org/linux/man-pages/man2/signalfd.2.html
	return signalfd(-1, &mySigset, 0);
#endif
}
