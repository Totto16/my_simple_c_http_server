
#include "../c_types.hpp"
#include "../cpp_types.hpp"
#include "./helpers.hpp"

#define IMPLEMENT_HPACK_TEST_CASE \
	TEST_CASE("testing hpack deserializing - external tests (" HPACK_TEST_CASE_VALUE ")") { \
\
		constexpr size_t DEFAULT_HEADER_TABLE_SIZE = 4096; \
\
		const auto hpack_cpp_global_handle = HpackGlobalHandle(); \
\
		const auto test_cases = get_thirdparty_hpack_test_cases(HPACK_TEST_CASE_VALUE); \
\
		for(const auto& test_case : test_cases) { \
\
			const auto case_str = std::string{ "Case " } + test_case.name; \
			doctest::String case_name = doctest::String{ case_str.c_str() }; \
\
			SUBCASE(case_name) { \
\
				INFO("test case description: ", test_case.description); \
\
				HpackStateCpp state = get_default_hpack_state_cpp(DEFAULT_HEADER_TABLE_SIZE); \
\
				REQUIRE_NE(state.get(), nullptr); \
\
				for(size_t i = 0; i < test_case.cases.size(); ++i) { \
\
					const auto& single_case = test_case.cases.at(i); \
\
					const auto case_str2 = std::string{ "Subcase " } + std::to_string(i); \
					doctest::String case_name2 = doctest::String{ case_str2.c_str() }; \
\
					INFO("the sequential number of that hpack packet has to be the same as the " \
					     "index: ", \
					     single_case.seqno, " | ", i); \
					REQUIRE_EQ(single_case.seqno, i); \
\
					SUBCASE(case_name2) { \
\
						const auto input = buffer_from_raw_data(single_case.wire_data); \
\
						const auto result = http2_hpack_decompress_data(state.get(), input); \
\
						std::string error = ""; \
						if(result.is_error) { \
							error = std::string{ result.data.error }; \
\
							INFO("Error occurred: ", error); \
							CHECK_FALSE(result.is_error); \
							continue; \
						} \
\
						REQUIRE_FALSE(result.is_error); \
\
						const auto actual_result = result.data.result; \
\
						const auto& expected_result = single_case.headers; \
\
						const auto actual_result_cpp = get_cpp_headers(actual_result); \
\
						REQUIRE_EQ(actual_result_cpp, expected_result); \
					} \
				} \
			} \
		} \
	}
