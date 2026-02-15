#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_REQUIRE_STRINGIFICATION_FOR_ALL_USED_TYPES
#define DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
#define DOCTEST_CONFIG_USE_STD_HEADERS
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

int main(int argc, char** argv) {
	setup_library();
	return doctest::Context(argc, argv).run();
}
