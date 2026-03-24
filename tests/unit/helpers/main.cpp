#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <utils/log.h>
#include <utils/number_parsing.h>

#include <support/helpers.hpp>

static void setup_global_timeout_multiplier() {

	const char* const env_timeout = getenv("DOCTEST_TEST_MULTIPLIER");

	if(env_timeout == NULL) {
		g_doctest_timeout_multiplier = 1;
		return;
	}

	bool success = false;

	const tstr_view env_timeout_view = tstr_view_from(env_timeout);

	const uint64_t env_timeout_val = parse_u64(env_timeout_view, &success);

	if(!success) {
		goto set_default;
	}

	if(SIZE_MAX != UINT64_MAX) {
		if(env_timeout_val > SIZE_MAX) {
			goto set_default;
		}
	}

	g_doctest_timeout_multiplier = (size_t)env_timeout_val;
	return;

set_default:
	g_doctest_timeout_multiplier = 1;
	return;
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
