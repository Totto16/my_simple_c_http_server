#include <doctest.h>

#include <support/generic.hpp>
#include <support/helpers.hpp>
#include <support/helpers/json.hpp>

#include <json/json.h>

#include "helpers/string_maker.hpp"

namespace {

struct JsonParseTestCase {
	std::string input;
	JsonVariant expected;
};

} // namespace

TEST_SUITE_BEGIN("json" * doctest::description("json tests") *
                 doctest::timeout(2.0 * g_doctest_timeout_multiplier));

TEST_CASE("testing parsing of json values <json_parser>") {

	std::vector<JsonParseTestCase> json_parse_test_cases = {
		JsonParseTestCase{ .input = "null", .expected = JsonVariantCpp::null() },
		JsonParseTestCase{ .input = "   null   ", .expected = JsonVariantCpp::null() },
		JsonParseTestCase{ .input = "\t			null   ", .expected = JsonVariantCpp::null() },
		JsonParseTestCase{ .input = "true", .expected = JsonVariantCpp::boolean(true) },
		JsonParseTestCase{ .input = "false", .expected = JsonVariantCpp::boolean(false) },
		JsonParseTestCase{ .input = "100", .expected = JsonVariantCpp::number((int64_t)100) },
		JsonParseTestCase{ .input = "-100", .expected = JsonVariantCpp::number((int64_t)-100) },
		JsonParseTestCase{ .input = "-100.01", .expected = JsonVariantCpp::number(-100.01) },
		JsonParseTestCase{ .input = "100.43", .expected = JsonVariantCpp::number(100.43) },
		JsonParseTestCase{ .input = "1e2", .expected = JsonVariantCpp::number((int64_t)100) },
		JsonParseTestCase{ .input = "1.2e3", .expected = JsonVariantCpp::number((int64_t)1200) },
		JsonParseTestCase{ .input = "1.3E+3", .expected = JsonVariantCpp::number((int64_t)1300) },
		JsonParseTestCase{ .input = "1.5E-2", .expected = JsonVariantCpp::number(0.015) },
		JsonParseTestCase{ .input = R"("hello world")",
		                   .expected = JsonVariantCpp::string("hello world") },
		JsonParseTestCase{ .input = R"("hello world\n\"\f\t")",
		                   .expected = JsonVariantCpp::string("hello world\n\"\f\t") },
	};

	for(const auto& test_case : json_parse_test_cases) {

		INFO("Test case: ", test_case.input);

		const tstr_view str_view = helpers::tstr_view_from_str(test_case.input);

		const auto parse_result = json_variant_parse_from_str(str_view);

		REQUIRE_IS_NOT_ERROR(parse_result);

		JsonVariant result = json_parse_result_get_as_ok(parse_result);
		CAutoFreePtr<JsonVariant> defer = { &result, free_json_variant };

		REQUIRE_EQ(result, test_case.expected);
	}
}

// TODO: compare withh nhlohmann json!

TEST_SUITE_END();
