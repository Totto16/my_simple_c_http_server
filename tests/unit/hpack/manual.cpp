#include <doctest.h>

#include "../c_types.hpp"
#include "./helpers.hpp"

TEST_CASE("testing hpack deserializing - manual tests") {

	constexpr size_t DEFAULT_HEADER_TABLE_SIZE = 4096;

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	const std::vector<ThirdPartyHpackTestCaseEntry> test_cases = {

	};

	HpackStateCpp state = get_default_hpack_state_cpp(DEFAULT_HEADER_TABLE_SIZE);

	REQUIRE_NE(state.get(), nullptr);

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str2 = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name2 = doctest::String{ case_str2.c_str() };

		INFO("the sequential number of that hpack packet has to be the same as the "
		     "index: ",
		     test_case.seqno, " | ", i);
		REQUIRE_EQ(test_case.seqno, i);

		SUBCASE(case_name2) {

			const auto input = buffer_from_raw_data(test_case.wire_data);

			const auto result = http2_hpack_decompress_data(state.get(), input);

			std::string error = "";
			if(result.is_error) {
				error = std::string{ result.data.error };

				INFO("Error occurred: ", error);
				CHECK_FALSE(result.is_error);
				continue;
			}

			REQUIRE_FALSE(result.is_error);

			const auto actual_result = result.data.result;

			const auto& expected_result = test_case.headers;

			const auto actual_result_cpp = get_cpp_headers(actual_result);

			REQUIRE_EQ(actual_result_cpp, expected_result);
		}
	}
}

struct IntegerTest {
	std::vector<std::uint8_t> values;
	std::uint8_t prefix_bits;
	HpackVariableInteger result;
};

TEST_CASE("testing hpack deserializing - integer tests") {

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.1
	const std::vector<IntegerTest> test_cases = {
		{ .values = { 0b01010 }, .prefix_bits = 5, .result = 10 },
		{ .values = { 0b11111, 0b10011010, 0b00001010 }, .prefix_bits = 5, .result = 1337 }
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str2 = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name2 = doctest::String{ case_str2.c_str() };

		SUBCASE(case_name2) {

			const auto input = buffer_from_raw_data(test_case.values);

			size_t pos = 0;

			const auto result = decode_hpack_variable_integer(
			    &pos, input.size, (std::uint8_t*)input.data, test_case.prefix_bits);

			std::string error = "";
			if(result.is_error) {
				error = std::string{ result.data.error };

				INFO("Error occurred: ", error);
			}

			REQUIRE_FALSE(result.is_error);

			const auto actual_result = result.data.value;

			const auto& expected_result = test_case.result;

			REQUIRE_EQ(actual_result, expected_result);

			REQUIRE_EQ(pos, input.size);
		}
	}
}
