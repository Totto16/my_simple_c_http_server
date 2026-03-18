#include <benchmark/benchmark.h>

#include "generated_hpack_tests.hpp"

#include <functional>

struct TestCaseStrCmp {
	std::string value;
	bool result;
};

static void StringCompareFast(const std::vector<TestCaseStrCmp>& test_cases,
                              const std::vector<std::string>& test_data_strings) {

	for(const auto& test_case : test_cases) {

		const bool expected_result = test_case.result;
		const auto& str = test_case.value;

		const auto actual_result = generated_c_test_fns_fast_string_compare_test_data(
		    tstr_view{ .data = str.c_str(), .len = str.size() });

		assert(actual_result.found == expected_result);

		if(actual_result.found) {
			assert(actual_result.index < test_data_strings.size());

			const std::string& actual_result_str = test_data_strings.at(actual_result.index);

			assert(actual_result_str == str);
		} else {
			assert(actual_result.index == 0);
		}
	}
}

static FastStringCompareResult normal_string_cmp(const std::vector<std::string>& strings,
                                                 const tstr_view str_view) {

	for(size_t i = 0; i < strings.size(); ++i) {
		const auto& str = strings.at(i);
		if(str.size() != str_view.len) {
			continue;
		}

		if(strncmp(str.c_str(), str_view.data, str.size()) == 0) {
			return FastStringCompareResult{ .found = true, .index = i };
		}
	}

	return FastStringCompareResult{ .found = false, .index = 0 };
}

static void StringCompareStandard(const std::vector<TestCaseStrCmp>& test_cases,
                                  const std::vector<std::string>& test_data_strings) {

	for(const auto& test_case : test_cases) {

		const bool expected_result = test_case.result;
		const auto& str = test_case.value;

		const auto actual_result = normal_string_cmp(
		    test_data_strings, tstr_view{ .data = str.c_str(), .len = str.size() });

		assert(actual_result.found == expected_result);

		if(actual_result.found) {
			assert(actual_result.index < test_data_strings.size());

			const std::string& actual_result_str = test_data_strings.at(actual_result.index);

			assert(actual_result_str == str);
		} else {
			assert(actual_result.index == 0);
		}
	}
}

static void BM_String_Compare(benchmark::State& state, bool fast) {

	std::vector<TestCaseStrCmp> test_cases = {
		TestCaseStrCmp{ .value = "", .result = false },
		TestCaseStrCmp{ .value = "hello world", .result = false },
		TestCaseStrCmp{ .value = "longer string", .result = false },
	};

	const auto test_data_strings = generated::c_test_fns::get_test_data_strings();

	for(const auto& val : test_data_strings) {
		test_cases.emplace_back(val, true);
	}

	const std::function<void(const std::vector<TestCaseStrCmp>& test_cases,
	                         const std::vector<std::string>& test_data_strings)>
	    cmp_func = fast ? StringCompareFast : StringCompareStandard;

	for(auto _ : state) {
		// This code gets timed

		cmp_func(test_cases, test_data_strings);
	}
}

static void BM_String_Compare_Fast(benchmark::State& state) {
	BM_String_Compare(state, true);
}

BENCHMARK(BM_String_Compare_Fast)->Name("string_compare/fast");

static void BM_String_Compare_Standard(benchmark::State& state) {
	BM_String_Compare(state, false);
}

BENCHMARK(BM_String_Compare_Standard)->Name("string_compare/standard");
