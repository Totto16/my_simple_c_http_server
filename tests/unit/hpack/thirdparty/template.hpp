
#include "../helpers.hpp"
#include "helpers/c_types.hpp"
#include "helpers/cpp_types.hpp"

#define IMPLEMENT_HPACK_TEST_CASE \
	TEST_CASE("testing hpack deserializing - external tests (" HPACK_TEST_CASE_VALUE \
	          ") <hpack_generated_ " HPACK_TEST_CASE_VALUE ">" * \
	          doctest::description("hpack thirdparty test case (" HPACK_TEST_CASE_VALUE ")") * \
	          doctest::timeout(10.0) * doctest::test_suite("hpack/thirdparty")) { \
\
		const auto hpack_cpp_global_handle = HpackGlobalHandle(); \
\
		const auto& test_cases = get_thirdparty_hpack_test_cases(HPACK_TEST_CASE_VALUE); \
\
		for(const auto& test_case : test_cases) { \
\
			const auto case_str = std::string{ "Case " } + test_case.name; \
			doctest::String case_name = doctest::String{ case_str.c_str() }; \
			SUBCASE(case_name) { \
				[&test_case]() -> void { \
					HpackDecompressStateCpp decompress_state = \
					    get_default_hpack_decompress_state_cpp(test_case.header_table_size); \
					REQUIRE_NE(decompress_state.get(), nullptr); \
\
					INFO("File: ", test_case.file); \
					INFO("test case description: ", test_case.description); \
\
					for(size_t i = 0; i < test_case.cases.size(); ++i) { \
\
						const auto& single_case = test_case.cases.at(i); \
\
						INFO("index: ", i, " seqno: ", single_case.seqno); \
\
						REQUIRE_EQ(single_case.seqno, i); \
\
						const auto input = buffer_from_raw_data(single_case.wire_data); \
\
						auto result = http2_hpack_decompress_data(decompress_state.get(), input); \
						CppDefer<Http2HpackDecompressResult> defer = { \
							&result, free_hpack_decompress_result \
						}; \
\
						REQUIRE_IS_NOT_ERROR(result); \
\
						const auto actual_result = result.data.result; \
\
						const auto& expected_result = single_case.headers; \
\
						const auto actual_result_cpp = get_cpp_headers(actual_result); \
\
						REQUIRE_EQ(actual_result_cpp, expected_result); \
					} \
				}(); \
			} \
		} \
	}
