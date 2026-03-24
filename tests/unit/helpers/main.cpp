#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <utils/log.h>
#include <utils/number_parsing.h>

#include <support/helpers.hpp>

static void setup_global_timeout_multiplier() {

	char* const env_timeout = getenv("DOCTEST_TEST_MULTIPLIER");

	if(env_timeout == NULL) {
		g_doctest_timeout_multiplier = 1;
		return;
	}

	bool success = false;

	const tstr env_timeout_tstr = tstr_own_cstr(env_timeout);

	const size_t env_timeout_val = parse_size_t(tstr_as_view(&env_timeout_tstr), &success);

	if(!success) {
		g_doctest_timeout_multiplier = 1;
		return;
	}

	g_doctest_timeout_multiplier = env_timeout_val;
}

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

	setup_global_timeout_multiplier();
}

int main(int argc, char** argv) {
	setup_library();
	return doctest::Context(argc, argv).run();
}
