
#include <support/helpers.hpp>
#include <support/helpers/hpack.hpp>

#include <doctest.h>

#include "helpers/json.hpp"

#define IMPLEMENT_HPACK_TEST_CASE \
	TEST_CASE("testing hpack deserializing - external tests (" HPACK_TEST_CASE_VALUE \
	          ") <hpack_generated_ " HPACK_TEST_CASE_VALUE ">" * \
	          doctest::description("hpack thirdparty test case (" HPACK_TEST_CASE_VALUE ")") * \
	          doctest::timeout(30.0) * doctest::test_suite("hpack/thirdparty")) { \
\
		const auto hpack_cpp_global_handle = hpack::HpackGlobalHandle(); \
\
		const auto& test_cases = hpack::get_thirdparty_hpack_test_cases(HPACK_TEST_CASE_VALUE); \
\
		for(const auto& test_case : test_cases) { \
\
			const auto case_str = std::string{ "Case " } + test_case.test_name; \
			doctest::String case_name = doctest::String{ case_str.c_str() }; \
			SUBCASE(case_name) { \
				[&test_case]() -> void { \
					hpack::HpackDecompressStateCpp decompress_state = \
					    hpack::get_default_hpack_decompress_state_cpp( \
					        test_case.header_mode.table_size); \
					REQUIRE_NE(decompress_state.get(), nullptr); \
\
					INFO("File: ", test_case.file); \
					INFO("test case description: ", test_case.description); \
\
					size_t current_header_table_size_expected = test_case.header_mode.table_size; \
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
						if(!test_case.header_mode.all_the_same && \
						   single_case.header_table_size.has_value()) { \
\
							/*the header table size sent in SETTINGS_HEADER_TABLE_SIZE and ACKed \
							 * just before this case. The first case should contain this field. If \
							 * omitted, the default value, 4,096, is used. */ \
							set_hpack_decompress_state_setting( \
							    decompress_state.get(), single_case.header_table_size.value()); \
						} \
\
						hpack::hacky_trick::HpackDecodingErrorStateHack error_state_stack{}; \
\
						auto result = http2_hpack_decompress_data(decompress_state.get(), input); \
						CAutoFreePtr<Http2HpackDecompressResult> defer = { \
							&result, hpack::free_hpack_decompress_result \
						}; \
\
						REQUIRE_IS_NOT_ERROR(result); \
\
						const auto actual_result = result.data.result; \
\
						const auto& expected_result = single_case.headers; \
\
						const auto actual_result_cpp = helpers::get_cpp_headers(actual_result); \
\
						REQUIRE_EQ(actual_result_cpp, expected_result); \
\
						REQUIRE_EQ(error_state_stack.get_errors(), \
						           single_case.strict_error_state); \
\
						const auto actual_dynamic_table = \
						    hpack::get_dynamic_decompress_table(decompress_state); \
\
						if(test_case.header_mode.all_the_same) { \
							/*assert that the size is correct*/ \
							REQUIRE_EQ(single_case.header_table_size, \
							           helpers::OptionalOr{ current_header_table_size_expected }); \
						} else { \
							/*assert that the correct size change happend*/ \
							if(single_case.header_table_size.has_value()) { \
								current_header_table_size_expected = \
								    single_case.header_table_size.value(); \
							} \
						} \
\
						/*assert that the size is correctly <= */ \
						REQUIRE_LE(actual_dynamic_table.size, current_header_table_size_expected); \
					} \
				}(); \
			} \
		} \
	}
