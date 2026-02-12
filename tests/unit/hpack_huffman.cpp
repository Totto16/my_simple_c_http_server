

#include <doctest.h>

#include <http/hpack_huffman.h>

#include "./generated/generated_hpack_tests.hpp"

#include "./c_types.hpp"

TEST_CASE("testing hpack deserializing (ascii)") {

	const auto* const tree = get_hpack_huffman_tree();

	REQUIRE_NE(tree, nullptr);

	const auto test_cases = generated::tests::test_cases;

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto test_case = test_cases.at(i);

		const auto case_str = std::string{ "Case " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {

			const auto raw_value = test_case.str;

			const SizedBuffer input = { .data = (void*)raw_value.data(), .size = raw_value.size() };

			const auto result = apply_huffman_code(tree, input);

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			REQUIRE_EQ(error, nullptr);

			const auto data = result.data.result;

			REQUIRE_EQ(data, test_case.encoded);
		}
	}
}
