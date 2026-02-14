#include "./tap_reporter.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <utils/log.h>

static void setup_library() {

	LogLevel log_level =
#ifdef NDEBUG
	    LogLevelError
#else
	    LogLevelTrace
#endif
	    ;

	initialize_logger();

	set_log_level(log_level);

	set_thread_name("main thread");
}

REGISTER_REPORTER("tap", 1, TapReporter);

int main(int argc, char** argv) {

	setup_library();
	return doctest::Context(argc, argv).run();
}
