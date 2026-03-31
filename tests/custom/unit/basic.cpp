#include <doctest.h>

#include <support/helpers.hpp>
#include <support/helpers/http.hpp>

#include <utils/number_parsing.h>

#include "helpers/string_maker.hpp"

namespace {
struct I64Result {
	bool success;
	std::int64_t value;

	[[nodiscard]] static I64Result error() {
		// the actual value is irrelevant, so I just used a random value, so that accidental checks
		// of this value result ine errors
		return I64Result{ .success = false, .value = -13513251 };
	}
};

struct I64TestCase {
	std::string input;
	I64Result expected;
};

struct U64Result {
	bool success;
	std::uint64_t value;

	[[nodiscard]] static U64Result error() {
		// the actual value is irrelevant, so I just used a random value, so that accidental checks
		// of this value result ine errors
		return U64Result{ .success = false, .value = 1412124UL };
	}
};

struct U64TestCase {
	std::string input;
	U64Result expected;
};

} // namespace

TEST_SUITE_BEGIN("basic" * doctest::description("basic tests") *
                 doctest::timeout(2.0 * g_doctest_timeout_multiplier));

TEST_CASE("testing parsing of i64 <i64_parser>") {

	std::vector<I64TestCase> i64_test_cases = {
		I64TestCase{ .input = "", .expected = I64Result::error() },
		I64TestCase{ .input = "+", .expected = I64Result::error() },
		I64TestCase{ .input = "-", .expected = I64Result::error() },
		I64TestCase{ .input = "-1212 ", .expected = I64Result::error() },
		I64TestCase{ .input = "12CAFFFE", .expected = I64Result::error() },
		I64TestCase{ .input = "12.12", .expected = I64Result::error() },
		I64TestCase{ .input = "12#121", .expected = I64Result::error() },
		I64TestCase{ .input = "0", .expected = I64Result{ .success = true, .value = 0 } },
		I64TestCase{ .input = "1", .expected = I64Result{ .success = true, .value = 1 } },
		I64TestCase{ .input = "+1424414",
		             .expected = I64Result{ .success = true, .value = 1424414L } },
		I64TestCase{ .input = "-42452",
		             .expected = I64Result{ .success = true, .value = -42452L } },
		I64TestCase{ .input = "4132513261326126426342734737457345754754745745745747",
		             .expected = I64Result::error() },
		I64TestCase{ .input = "18446744073709551615", .expected = I64Result::error() },
		I64TestCase{ .input = "9223372036854775808", .expected = I64Result::error() },
		I64TestCase{ .input = "9223372036854775807",
		             .expected = I64Result{ .success = true,
		                                    .value = std::numeric_limits<std::int64_t>::max() } },
		I64TestCase{ .input = "-18446744073709551615", .expected = I64Result::error() },
		I64TestCase{ .input = "-9223372036854775808",
		             .expected = I64Result{ .success = true,
		                                    .value = std::numeric_limits<std::int64_t>::min() } },
	};

	for(const auto& test_case : i64_test_cases) {

		INFO("Test case: ", test_case.input);

		auto input = tstr_from_string(test_case.input);
		CAutoFreePtr<tstr> defer_tstr = { &input, tstr_free };

		bool success = false;
		const std::int64_t result = parse_i64(tstr_as_view(&input), &success);

		REQUIRE_EQ(success, test_case.expected.success);

		if(success) {
			REQUIRE_EQ(result, test_case.expected.value);
		}
	}
}
TEST_CASE("testing parsing of u64 <u64_parser>") {

	std::vector<U64TestCase> u64_test_cases = {
		U64TestCase{ .input = "", .expected = U64Result::error() },
		U64TestCase{ .input = "+", .expected = U64Result::error() },
		U64TestCase{ .input = "-", .expected = U64Result::error() },
		U64TestCase{ .input = "-1212 ", .expected = U64Result::error() },
		U64TestCase{ .input = "12CAFFFE", .expected = U64Result::error() },
		U64TestCase{ .input = "12.12", .expected = U64Result::error() },
		U64TestCase{ .input = "12#121", .expected = U64Result::error() },
		U64TestCase{ .input = "0", .expected = U64Result{ .success = true, .value = 0 } },
		U64TestCase{ .input = "1", .expected = U64Result{ .success = true, .value = 1 } },
		// this is intentional, we don#t want any usage of "+" here
		U64TestCase{ .input = "+1424414", .expected = U64Result::error() },
		U64TestCase{ .input = "-42452", .expected = U64Result::error() },
		U64TestCase{ .input = "4132513261326126426342734737457345754754745745745747",
		             .expected = U64Result::error() },
		U64TestCase{ .input = "18446744073709551616", .expected = U64Result::error() },
		U64TestCase{ .input = "9223372036854775808",
		             .expected = U64Result{ .success = true, .value = 9223372036854775808ULL } },
		U64TestCase{ .input = "18446744073709551615",
		             .expected = U64Result{ .success = true,
		                                    .value = std::numeric_limits<std::uint64_t>::max() } },
		U64TestCase{ .input = "-18446744073709551615", .expected = U64Result::error() },
		U64TestCase{ .input = "0",
		             .expected = U64Result{ .success = true,
		                                    .value = std::numeric_limits<std::uint64_t>::min() } },
	};

	for(const auto& test_case : u64_test_cases) {

		INFO("Test case: ", test_case.input);

		auto input = tstr_from_string(test_case.input);
		CAutoFreePtr<tstr> defer_tstr = { &input, tstr_free };

		bool success = false;
		const std::uint64_t result = parse_u64(tstr_as_view(&input), &success);

		REQUIRE_EQ(success, test_case.expected.success);

		if(success) {
			REQUIRE_EQ(result, test_case.expected.value);
		}
	}
}

TEST_SUITE_END();
