#include <doctest.h>

#include <support/generic.hpp>
#include <support/helpers.hpp>
#include <support/helpers/json.hpp>

#include <json/json.h>

#include "helpers/string_maker.hpp"

namespace {

struct JsonParseTestCaseSuccess {
	std::string input;
	JsonValue expected;
};

struct JsonParseTestCaseError {
	std::string input;
	JsonErrorCpp expected_error;
};

} // namespace

TEST_SUITE_BEGIN("json" * doctest::description("json tests") *
                 doctest::timeout(2.0 * g_doctest_timeout_multiplier));

TEST_CASE("testing parsing of json values <json_parser>") {

	std::vector<JsonParseTestCaseSuccess> json_parse_test_cases = {
		JsonParseTestCaseSuccess{ .input = "null", .expected = JsonValueCpp::null() },
		JsonParseTestCaseSuccess{ .input = "   null   ", .expected = JsonValueCpp::null() },
		JsonParseTestCaseSuccess{ .input = "\t			null   ",
		                          .expected = JsonValueCpp::null() },
		JsonParseTestCaseSuccess{ .input = "true", .expected = JsonValueCpp::boolean(true) },
		JsonParseTestCaseSuccess{ .input = "false", .expected = JsonValueCpp::boolean(false) },
		JsonParseTestCaseSuccess{ .input = "100", .expected = JsonValueCpp::number((int64_t)100) },
		JsonParseTestCaseSuccess{ .input = "-100",
		                          .expected = JsonValueCpp::number((int64_t)-100) },
		JsonParseTestCaseSuccess{ .input = "-100.01", .expected = JsonValueCpp::number(-100.01) },
		JsonParseTestCaseSuccess{ .input = "100.43", .expected = JsonValueCpp::number(100.43) },
		JsonParseTestCaseSuccess{ .input = "1e2", .expected = JsonValueCpp::number((int64_t)100) },
		JsonParseTestCaseSuccess{ .input = "1.2e3",
		                          .expected = JsonValueCpp::number((int64_t)1200) },
		JsonParseTestCaseSuccess{ .input = "1.3E+3",
		                          .expected = JsonValueCpp::number((int64_t)1300) },
		JsonParseTestCaseSuccess{ .input = "1.5E-2", .expected = JsonValueCpp::number(0.015) },
		JsonParseTestCaseSuccess{ .input = R"("hello world")",
		                          .expected = JsonValueCpp::string("hello world") },
		JsonParseTestCaseSuccess{ .input = R"("hello world\n\"\f\t")",
		                          .expected = JsonValueCpp::string("hello world\n\"\f\t") },
		JsonParseTestCaseSuccess{
		    .input = R"([null,  	1,2,   true ])",
		    .expected = JsonValueCpp::array(
		        { JsonValueCpp::null(), JsonValueCpp::number((int64_t)1),
		          JsonValueCpp::number((int64_t)2), JsonValueCpp::boolean(true) }) },
		JsonParseTestCaseSuccess{
		    .input =
		        R"({"key1": "hello", "key2": null, "nested": { "nested_key"   : {"nested_key2": true, "array": []}}})",
		    .expected = JsonValueCpp::object(
		        { { "key1", JsonValueCpp::string("hello") },
		          { "key2", JsonValueCpp::null() },
		          { "nested",
		            JsonValueCpp::object({
		                { "nested_key", JsonValueCpp::object({
		                                    { "nested_key2", JsonValueCpp::boolean(true) },
		                                    { "array", JsonValueCpp::array({}) },
		                                }) },
		            }

		                                 ) } }) },
	};
	CAutoFreePtr<std::vector<JsonParseTestCaseSuccess>> defer_tests = {
		&json_parse_test_cases,
		[](std::vector<JsonParseTestCaseSuccess>* const values) -> void {
		    for(size_t i = 0; i < values->size(); ++i) {
			    auto* const value = &(values->at(i));
			    free_json_value(&(value->expected));
		    }
		}
	};

	for(const auto& test_case : json_parse_test_cases) {

		INFO("Test case: ", test_case.input);

		const tstr_view str_view = helpers::tstr_view_from_str(test_case.input);

		const auto parse_result = json_value_parse_from_str(str_view);

		REQUIRE_EQ(get_current_tag_type_for_json_parse_result(parse_result), JsonParseResultTypeOk);

		JsonValue result = json_parse_result_get_as_ok(parse_result);
		CAutoFreePtr<JsonValue> defer = { &result, free_json_value };

		REQUIRE_EQ(result, test_case.expected);
	}
}

TEST_CASE("testing parse errors of json values <json_parser_error>") {

	// just here as a dummy tstr_view
	const auto dummy_str_view = tstr_view_from("__dummy_str_view__impl__");

	std::vector<JsonParseTestCaseError> json_parse_test_cases = {
		JsonParseTestCaseError{
		    .input = "not_null xD",
		    .expected_error = JsonErrorCpp::with_string_loc(
		        "not null", dummy_str_view, SourcePosition{ .line = 0, .col = 0 }) },
		JsonParseTestCaseError{
		    .input = "for_sure_not_false ",
		    .expected_error = JsonErrorCpp::with_string_loc(
		        "not a boolean", dummy_str_view, SourcePosition{ .line = 0, .col = 0 }) },
		JsonParseTestCaseError{
		    .input = "trivially_not_true ",
		    .expected_error = JsonErrorCpp::with_string_loc(
		        "not a boolean", dummy_str_view, SourcePosition{ .line = 0, .col = 0 }) },
		JsonParseTestCaseError{
		    .input = "  trivially_not_true ",
		    .expected_error = JsonErrorCpp::with_string_loc(
		        "not a boolean", dummy_str_view, SourcePosition{ .line = 0, .col = 2 }) },
		JsonParseTestCaseError{
		    .input = "  \ntrivially_not_true ",
		    .expected_error = JsonErrorCpp::with_string_loc(
		        "not a boolean", dummy_str_view, SourcePosition{ .line = 1, .col = 0 }) },
		JsonParseTestCaseError{
		    .input = "\n  trivially_not_true ",
		    .expected_error = JsonErrorCpp::with_string_loc(
		        "not a boolean", dummy_str_view, SourcePosition{ .line = 1, .col = 2 }) },
		JsonParseTestCaseError{ .input = R"({"key1": 1, "key1": 2 })",
		                        .expected_error = JsonErrorCpp::with_string_loc(
		                            "json object has duplicate key", dummy_str_view,
		                            SourcePosition{ .line = 1, .col = 2 }) },
	};

	for(const auto& test_case : json_parse_test_cases) {

		INFO("Test case: ", test_case.input);

		const tstr_view str_view = helpers::tstr_view_from_str(test_case.input);

		const auto parse_result = json_value_parse_from_str(str_view);

		REQUIRE_EQ(get_current_tag_type_for_json_parse_result(parse_result),
		           JsonParseResultTypeError);

		JsonError result = json_parse_result_get_as_error(parse_result);

		const auto actual_error = JsonErrorCpp{ result };

		REQUIRE_EQ(actual_error, test_case.expected_error);
	}
}

TEST_CASE("testing helper functions of the json parser <json_parser_helper_fn>") {

	SUBCASE("null source handling") {
		[]() -> void {
			const auto null_src = make_null_source_location();

			REQUIRE_TRUE(is_null_source_location(null_src));

			const tstr_static nonnull_src_str = "nothing"_tstr_static;

			const auto nonnull_src =
			    SourceLocation{ .source = new_json_source_string(JsonStringSource{
				                    .data = tstr_static_as_view(nonnull_src_str) }),
				                .pos = SourcePosition{ .line = 0, .col = 0 } };

			REQUIRE_FALSE(is_null_source_location(nonnull_src));
		}();
	}

	SUBCASE("object add entry") {
		[]() -> void {
			JsonObject* const object = get_empty_json_object();

			if(object == nullptr) {
				throw std::runtime_error("JSON object initialization failed");
			}

			{
				const auto key = "key_null"_tstr;

				const auto add_result =
				    json_object_add_entry_tstr(object, &key, JsonValueCpp::null());
				if(!tstr_static_is_null(add_result)) {
					throw std::runtime_error(
					    std::string{ "JSON object entry addition failed for key: " } +
					    string_from_tstr_static(add_result));
				}
			}

			{

				const auto key = "key_2";

				const auto add_result =
				    json_object_add_entry_cstr(object, key, JsonValueCpp::number((int64_t)2));
				if(!tstr_static_is_null(add_result)) {
					throw std::runtime_error(
					    std::string{ "JSON object entry addition failed for key: " } +
					    string_from_tstr_static(add_result));
				}
			}

			auto json_value = new_json_value_object(object);

			auto expected_value = JsonValueCpp::object({
			    { "key_null", JsonValueCpp::null() },
			    { "key_2", JsonValueCpp::number((int64_t)2) },
			});

			REQUIRE_EQ(json_value, expected_value);

			free_json_value(&json_value);
			free_json_value(&expected_value);
		}();
	}
}

// TODO: compare with nhlohmann json!

TEST_SUITE_END();
