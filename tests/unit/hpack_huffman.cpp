

#include <doctest.h>

#include <http/hpack_huffman.h>

#include "generated_hpack_tests.hpp"

#include "./c_types.hpp"

[[nodiscard]] static SizedBuffer buffer_from_raw_data(const std::vector<std::uint8_t>& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

[[nodiscard]] static SizedBuffer buffer_from_str(const std::string& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

struct TestCaseManual {
	std::vector<std::uint8_t> encoded;
	std::string str;
};

TEST_CASE("testing hpack huffman deserializing - from hpack spec") {

	const auto* const tree = get_hpack_huffman_tree();

	REQUIRE_NE(tree, nullptr);

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4
	const std::vector<TestCaseManual> test_cases = {
		TestCaseManual{
		    .encoded = { 0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff },
		    .str = "www.example.com" },
		TestCaseManual{ .encoded = { 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf }, .str = "no-cache" },
		TestCaseManual{ .encoded = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f },
		                .str = "custom-key" },
		TestCaseManual{ .encoded = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf },
		                .str = "custom-value" },
		TestCaseManual{ .encoded = { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44,
		                             0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
		                             0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff },
		                .str = "Mon, 21 Oct 2013 20:13:21 GMT" },
		TestCaseManual{ .encoded = { 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97,
		                             0xc8, 0xe9, 0xae, 0x82, 0xae, 0x43, 0xd3 },
		                .str = "https://www.example.com" },
		TestCaseManual{ .encoded = { 0x64, 0x0e, 0xff }, .str = "307" },
		TestCaseManual{ .encoded = { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44,
		                             0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
		                             0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff },
		                .str = "Mon, 21 Oct 2013 20:13:22 GMT" },
		TestCaseManual{ .encoded = { 0x9b, 0xd9, 0xab }, .str = "gzip" },
		TestCaseManual{ .encoded = { 0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3,
		                             0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60, 0xd5, 0xaf,
		                             0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f,
		                             0xb5, 0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0,
		                             0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07 },
		                .str = "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1" },
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto test_case = test_cases.at(i);

		const auto case_str = std::string{ "Case " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {

			const auto input = buffer_from_raw_data(test_case.encoded);

			const auto result = decode_bytes_huffman(tree, input);

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			REQUIRE_EQ(error, nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_str(test_case.str);

			const std::string actual_result_str =
			    std::string{ (char*)actual_result.data, actual_result.size };

			INFO("the actual encoded string: ", test_case.str);
			INFO("the decoded string: ", actual_result_str);
			REQUIRE_EQ(actual_result, expected_result);
		}
	}
}

TEST_CASE("testing hpack huffman deserializing (ascii) - generated") {

	const auto* const tree = get_hpack_huffman_tree();

	REQUIRE_NE(tree, nullptr);

	const auto test_cases = generated::tests::test_cases_ascii;

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto test_case = test_cases.at(i);

		const auto case_str = std::string{ "Case " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {

			const auto input = buffer_from_raw_data(test_case.encoded);

			const auto result = decode_bytes_huffman(tree, input);

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			REQUIRE_EQ(error, nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_str(test_case.str);

			const std::string actual_result_str =
			    std::string{ (char*)actual_result.data, actual_result.size };

			INFO("the actual encoded string: ", test_case.str);
			INFO("the decoded string: ", actual_result_str);
			REQUIRE_EQ(actual_result, expected_result);
		}
	}
}

TEST_CASE("testing hpack huffman deserializing (utf8) - generated") {

	const auto* const tree = get_hpack_huffman_tree();

	REQUIRE_NE(tree, nullptr);

	const auto test_cases = generated::tests::test_cases_utf8;

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto test_case = test_cases.at(i);

		const auto case_str = std::string{ "Case " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {

			const auto input = buffer_from_raw_data(test_case.encoded);

			const auto result = decode_bytes_huffman(tree, input);

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			REQUIRE_EQ(error, nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_raw_data(test_case.value);

			REQUIRE_EQ(actual_result, expected_result);
		}
	}
}
