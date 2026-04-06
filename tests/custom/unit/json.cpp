#include <doctest.h>

#include <support/generic.hpp>
#include <support/helpers.hpp>
#include <support/helpers/json.hpp>

#include <json/json.h>

#include "helpers/string_maker.hpp"

namespace {

struct JsonParseTestCaseSuccess {
	std::string input;
	JsonVariant expected;
};

struct JsonParseTestCaseError {
	std::string input;
	std::string expected_error;
};

} // namespace

TEST_SUITE_BEGIN("json" * doctest::description("json tests") *
                 doctest::timeout(2.0 * g_doctest_timeout_multiplier));

TEST_CASE("testing parsing of json values <json_parser>") {

	std::vector<JsonParseTestCaseSuccess> json_parse_test_cases = {
		JsonParseTestCaseSuccess{ .input = "null", .expected = JsonVariantCpp::null() },
		JsonParseTestCaseSuccess{ .input = "   null   ", .expected = JsonVariantCpp::null() },
		JsonParseTestCaseSuccess{ .input = "\t			null   ",
		                          .expected = JsonVariantCpp::null() },
		JsonParseTestCaseSuccess{ .input = "true", .expected = JsonVariantCpp::boolean(true) },
		JsonParseTestCaseSuccess{ .input = "false", .expected = JsonVariantCpp::boolean(false) },
		JsonParseTestCaseSuccess{ .input = "100",
		                          .expected = JsonVariantCpp::number((int64_t)100) },
		JsonParseTestCaseSuccess{ .input = "-100",
		                          .expected = JsonVariantCpp::number((int64_t)-100) },
		JsonParseTestCaseSuccess{ .input = "-100.01", .expected = JsonVariantCpp::number(-100.01) },
		JsonParseTestCaseSuccess{ .input = "100.43", .expected = JsonVariantCpp::number(100.43) },
		JsonParseTestCaseSuccess{ .input = "1e2",
		                          .expected = JsonVariantCpp::number((int64_t)100) },
		JsonParseTestCaseSuccess{ .input = "1.2e3",
		                          .expected = JsonVariantCpp::number((int64_t)1200) },
		JsonParseTestCaseSuccess{ .input = "1.3E+3",
		                          .expected = JsonVariantCpp::number((int64_t)1300) },
		JsonParseTestCaseSuccess{ .input = "1.5E-2", .expected = JsonVariantCpp::number(0.015) },
		JsonParseTestCaseSuccess{ .input = R"("hello world")",
		                          .expected = JsonVariantCpp::string("hello world") },
		JsonParseTestCaseSuccess{ .input = R"("hello world\n\"\f\t")",
		                          .expected = JsonVariantCpp::string("hello world\n\"\f\t") },
		JsonParseTestCaseSuccess{
		    .input = R"([null,  	1,2,   true ])",
		    .expected = JsonVariantCpp::array(
		        { JsonVariantCpp::null(), JsonVariantCpp::number((int64_t)1),
		          JsonVariantCpp::number((int64_t)2), JsonVariantCpp::boolean(true) }) },
		JsonParseTestCaseSuccess{
		    .input =
		        R"({"key1": "hello", "key2": null, "nested": { "nested_key"   : {"nested_key2": true, "array": []}}})",
		    .expected = JsonVariantCpp::object(
		        { { "key1", JsonVariantCpp::string("hello") },
		          { "key2", JsonVariantCpp::null() },
		          { "nested",
		            JsonVariantCpp::object({
		                { "nested_key", JsonVariantCpp::object({
		                                    { "nested_key2", JsonVariantCpp::boolean(true) },
		                                    { "array", JsonVariantCpp::array({}) },
		                                }) },
		            }

		                                   ) } }) },
	};
	CAutoFreePtr<std::vector<JsonParseTestCaseSuccess>> defer_tests = {
		&json_parse_test_cases,
		[](std::vector<JsonParseTestCaseSuccess>* const values) -> void {
		    for(size_t i = 0; i < values->size(); ++i) {
			    auto* const value = &(values->at(i));
			    free_json_variant(&(value->expected));
		    }
		}
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

TEST_CASE("testing parse errors of json values <json_parser_error>") {

	std::vector<JsonParseTestCaseError> json_parse_test_cases = {
		JsonParseTestCaseError{ .input = "not_null xD", .expected_error = "not null" },
		JsonParseTestCaseError{ .input = "for_sure_not_false ", .expected_error = "not a boolean" },
		JsonParseTestCaseError{ .input = "trivially_not_true ", .expected_error = "not a boolean" },
	};

	for(const auto& test_case : json_parse_test_cases) {

		INFO("Test case: ", test_case.input);

		const tstr_view str_view = helpers::tstr_view_from_str(test_case.input);

		const auto parse_result = json_variant_parse_from_str(str_view);

		REQUIRE_IS_ERROR(parse_result);

		tstr_static result = json_parse_result_get_as_error(parse_result).error;

		const auto actual_error = string_from_tstr_static(result);

		REQUIRE_EQ(actual_error, test_case.expected_error);
	}
}

// TODO: compare with nhlohmann json!

TEST_SUITE_END();
