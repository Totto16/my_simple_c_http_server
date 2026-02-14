#include <doctest.h>

#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdynamic-exception-spec"

#include <tap++/tap++.h>

#pragma GCC diagnostic pop

using namespace doctest;

struct TapReporter : public IReporter {
	// caching pointers/references to objects of these types - safe to do
	std::ostream& stdout_stream;
	const ContextOptions& opt;
	const TestCaseData* tc;
	std::mutex mutex;

	// constructor has to accept the ContextOptions by ref as a single argument
	TapReporter(const ContextOptions& in) : stdout_stream(*in.cout), opt(in) {
		TAP::set_output(*in.cout);
	}

	void report_query(const QueryData& /*in*/) override {}

	void test_run_start() override {
		std::cout << "start RUN test\n";
	}

	void test_run_end(const TestRunStats& /* in */) override { 
			std::cout << "end RUN test\n";
		//TAP::done_testing(in.numTestCases);
	 }

	void test_case_start(const TestCaseData& /* in */) override {
		std::cout << "test CASE start\n";
		/* TAP::plan(1);
		if(in.m_skip) {
			TAP::skip(1, in.m_name);
		} */
	}

	// called when a test case is reentered because of unfinished subcases
	void test_case_reenter(const TestCaseData& /*in*/) override {}

	void test_case_end(const CurrentTestCaseStats& /* in */) override {
			std::cout << "test CASE end\n";
		/* if(in.testCaseSuccess){
			TAP::pass("unkown 1");
		}else{
			TAP::fail("unkown 2");
		} */
	}

	void test_case_exception(const TestCaseException& /*in*/) override {}

	void subcase_start(const SubcaseSignature& /*in*/) override {
		std::lock_guard<std::mutex> lock(mutex);
				std::cout << "test SUB start\n";
	}

	void subcase_end() override { std::lock_guard<std::mutex> lock(mutex);
		std::cout << "test SUB end\n";
	}

	void log_assert(const AssertData& in) override {
		// don't include successful asserts by default - this is done here
		// instead of in the framework itself because doctest doesn't know
		// if/when a reporter/listener cares about successful results
		if(!in.m_failed && !opt.success) return;

		// make sure there are no races - this is done here instead of in the
		// framework itself because doctest doesn't know if reporters/listeners
		// care about successful asserts and thus doesn't lock a mutex unnecessarily
		std::lock_guard<std::mutex> lock(mutex);

		// ...
	}

	void log_message(const MessageData& /*in*/) override {
		// messages too can be used in a multi-threaded context - like asserts
		std::lock_guard<std::mutex> lock(mutex);

		// ...
	}

	void test_case_skipped(const TestCaseData& /*in*/) override {
		TAP::skip(1);
	}
};
