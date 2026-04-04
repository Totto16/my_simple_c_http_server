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
		JsonParseTestCase{ .input = "null", .expected = new_json_variant_null() },
		JsonParseTestCase{ .input = "   null   ", .expected = new_json_variant_null() },
		JsonParseTestCase{ .input = "\t			null   ", .expected = new_json_variant_null() },
		JsonParseTestCase{ .input = "true",
		                   .expected = new_json_variant_boolean(JsonBoolean{ .value = true }) },
		JsonParseTestCase{ .input = "false",
		                   .expected = new_json_variant_boolean(JsonBoolean{ .value = false }) },
		JsonParseTestCase{ .input = "100",
		                   .expected = new_json_variant_number(JsonNumber{ .value = 100 }) },
		JsonParseTestCase{ .input = "-100",
		                   .expected = new_json_variant_number(JsonNumber{ .value = -100 }) },
		JsonParseTestCase{ .input = "-100.01",
		                   .expected = new_json_variant_number(JsonNumber{ .value = -100.01 }) },
		JsonParseTestCase{ .input = "100.43",
		                   .expected = new_json_variant_number(JsonNumber{ .value = 100.43 }) },
		JsonParseTestCase{ .input = "1e2",
		                   .expected = new_json_variant_number(JsonNumber{ .value = 100 }) },
		JsonParseTestCase{ .input = "1.2e3",
		                   .expected = new_json_variant_number(JsonNumber{ .value = 1200 }) },
		JsonParseTestCase{ .input = "1.2E+3",
		                   .expected = new_json_variant_number(JsonNumber{ .value = 1200 }) },
		JsonParseTestCase{ .input = "1.2E-2",
		                   .expected = new_json_variant_number(JsonNumber{ .value = 0.012 }) },
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
