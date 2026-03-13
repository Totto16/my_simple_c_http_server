#pragma once

#include "helpers/cpp_types.hpp"
#include <http/hpack.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

struct ThirdPartyHpackTestCaseEntry {
	size_t seqno;
	std::vector<std::uint8_t> wire_data;
	std::vector<std::pair<std::string, std::string>> headers;
	std::optional<size_t>
	    header_table_size; // the header table size sent in SETTINGS_HEADER_TABLE_SIZE and ACKed
	                       // just before this case. The first case should contain this field. If
	                       // omitted, the default value, 4,096, is used.
	std::vector<std::string> strict_error_state; // strict key errors
};

struct HeaderTableMode {
	bool all_the_same;
	size_t table_size;
};

struct ThirdPartyHpackTestCase {
	std::string description;
	std::vector<ThirdPartyHpackTestCaseEntry> cases;
	std::string test_name;
	HeaderTableMode header_mode;
	std::filesystem::path file;
};

#define TEST_ENV_PREFIX "TOTTO_SIMPLE_HTTP_SERVER___ENV___HACK_IMPL"

static void test_framework_setup_global_data() {

	setenv(TEST_ENV_PREFIX "_IS_TEST", "TRUE", 1);
	setenv(TEST_ENV_PREFIX "_STRICT_QUIET", "FALSE", 1);
}

static void test_framework_free_global_data() {

	unsetenv(TEST_ENV_PREFIX "_IS_TEST");
	unsetenv(TEST_ENV_PREFIX "_STRICT_QUIET");
}

struct HpackGlobalHandle {

	HpackGlobalHandle() {

		global_initialize_http2_hpack_data();
		test_framework_setup_global_data();
	}

	HpackGlobalHandle(HpackGlobalHandle&&) = delete;

	HpackGlobalHandle(const HpackGlobalHandle&) = delete;

	HpackGlobalHandle& operator=(const HpackGlobalHandle&) = delete;

	HpackGlobalHandle operator=(HpackGlobalHandle&&) = delete;

	~HpackGlobalHandle() {
		global_free_http2_hpack_data();
		test_framework_free_global_data();
	}
};

struct HpackDecodingErrorStateHack {
	HpackDecodingErrorStateHack() {

#ifdef NDEBUG
	#error \
	    "this class doesn't work in non debug mode, as than the library doesn't use thsi expensive global tracking"
#endif

		// set quiet, as we collect the result
		setenv(TEST_ENV_PREFIX "_STRICT_QUIET", "TRUE", 1);

		setenv(TEST_ENV_PREFIX "_TEST_CASE_FAILED_KEY_NAMES", "", 1);
	}

	HpackDecodingErrorStateHack(HpackDecodingErrorStateHack&&) = delete;

	HpackDecodingErrorStateHack(const HpackDecodingErrorStateHack&) = delete;

	HpackDecodingErrorStateHack& operator=(const HpackDecodingErrorStateHack&) = delete;

	HpackDecodingErrorStateHack operator=(HpackDecodingErrorStateHack&&) = delete;

	~HpackDecodingErrorStateHack() {
		setenv(TEST_ENV_PREFIX "_STRICT_QUIET", "FALSE", 1);
		unsetenv(TEST_ENV_PREFIX "_TEST_CASE_FAILED_KEY_NAMES");
	}

	[[nodiscard]] std::vector<std::string> get_errors() {

		std::vector<std::string> result{};

		const char* const values = getenv(TEST_ENV_PREFIX "_TEST_CASE_FAILED_KEY_NAMES");

		if(values == NULL) {
			return result;
		}

		std::string cpp_values = std::string{ values };

		if(cpp_values == "") {
			return result;
		}

		size_t start = 0;
		size_t end;

		while((end = cpp_values.find('\1', start)) != std::string::npos) {
			result.push_back(cpp_values.substr(start, end - start));
			start = end + 1;
		}

		result.push_back(cpp_values.substr(start));

		return result;
	}
};

[[nodiscard]] static std::uint8_t parse_hex_byte(const char& val) {

	if(val >= '0' && val <= '9') {
		return val - '0';
	}

	if(val >= 'a' && val <= 'f') {
		return 10 + (val - 'a');
	}

	if(val >= 'A' && val <= 'F') {
		return 10 + (val - 'A');
	}

	throw std::runtime_error("invalid byte data");
}

[[nodiscard]] static std::vector<std::uint8_t> parse_wire_data(const std::string& raw_wire) {

	std::vector<std::uint8_t> result{};

	if(raw_wire.length() % 2 != 0) {
		throw std::runtime_error(std::string{ "invalid wire data: length is " } +
		                         std::to_string(raw_wire.length()));
	}

	for(size_t i = 0; i < raw_wire.length(); i += 2) {

		const auto first_byte = parse_hex_byte(raw_wire.at(i));
		const auto second_byte = parse_hex_byte(raw_wire.at(i + 1));

		result.push_back((first_byte << 4) + second_byte);
	}

	return result;
}

[[nodiscard]] static std::vector<std::pair<std::string, std::string>>
parse_headers_map(const nlohmann::json& value) {

	std::vector<std::pair<std::string, std::string>> result{};

	if(!value.is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(const auto& val : value) {

		if(!val.is_object()) {
			throw std::runtime_error("json is malformed");
		}

		for(auto& el : val.items()) {
			result.emplace_back(el.key(), el.value());
		}
	}

	return result;
}

[[nodiscard]] static std::optional<size_t> get_optional_header_size(const nlohmann::json& case_) {

	if(!case_.contains("header_table_size")) {
		return std::nullopt;
	}

	const auto& header_table_size_v = case_["header_table_size"];

	if(header_table_size_v.is_null()) {
		return std::nullopt;
	}

	return header_table_size_v.get<size_t>();
}

namespace consts {
struct StrictErrorException {
	std::string suite_name;
	std::string test_name;
	size_t seqno;
	std::string field_name;

	[[nodiscard]] bool operator==(const StrictErrorException& lhs) const {
		if(this->suite_name != lhs.suite_name) {
			return false;
		}

		if(this->test_name != lhs.test_name) {
			return false;
		}

		if(this->seqno != lhs.seqno) {
			return false;
		}

		return this->field_name == lhs.field_name;
	}
};

static std::vector<StrictErrorException> strict_error_state_exceptions = {
	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal
	StrictErrorException{
	    .suite_name = "nghttp2-change-table-size",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing"
	StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing"
	StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing",
	// searched for by using "636f6e74656e742d7479706500" alias "content-type" in ascii and than 00
	StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_30",
	    .seqno = 138,
	    .field_name = "content-type",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing",
	// searched for by using "636f6e74656e742d7479706500" alias "content-type" in ascii and than 00
	StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_30",
	    .seqno = 599,
	    .field_name = "content-type",
	},

	// TODO: investigate
	StrictErrorException{
	    .suite_name = "haskell-http2-linear-huffman",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	StrictErrorException{
	    .suite_name = "python-hpack",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	StrictErrorException{
	    .suite_name = "haskell-http2-linear",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	StrictErrorException{
	    .suite_name = "go-hpack",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	StrictErrorException{
	    .suite_name = "haskell-http2-static-huffman",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate, likely legit!
	StrictErrorException{
	    .suite_name = "nghttp2-16384-4096",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// TODO: investigate, likely legit!
	StrictErrorException{
	    .suite_name = "node-http2-hpack",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// TODO: investigate, likely legit!
	StrictErrorException{
	    .suite_name = "nghttp2",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	}

};

} // namespace consts

#define DEFAULT_HEADER_TABLE_SIZE 4096

[[nodiscard]] static ThirdPartyHpackTestCaseEntry get_case_from_json(const nlohmann::json& value,
                                                                     const std::string& suite_name,
                                                                     const std::string& test_name) {

	size_t seqno = value["seqno"].get<size_t>();

	const std::string raw_wire_data = value["wire"].get<std::string>();

	const auto wire_data = parse_wire_data(raw_wire_data);

	const auto headers = parse_headers_map(value["headers"]);

	const auto header_table_size = get_optional_header_size(value);

	std::vector<std::string> strict_error_state{};

	for(const auto& header : headers) {
		if(header.second == "") {

			// some exceptions, as they aree either encoded as 0 byte string and not using an
			// invalid entry value (NULL != "")
			if(vec_contains(consts::strict_error_state_exceptions, consts::StrictErrorException{
			                                                           .suite_name = suite_name,
			                                                           .test_name = test_name,
			                                                           .seqno = seqno,
			                                                           .field_name = header.first,
			                                                       })) {
				continue;
			}

			strict_error_state.emplace_back(header.first);
		}
	}

	return ThirdPartyHpackTestCaseEntry{
		.seqno = seqno,
		.wire_data = wire_data,
		.headers = headers,
		.header_table_size = header_table_size.value_or(DEFAULT_HEADER_TABLE_SIZE),
		.strict_error_state = strict_error_state,
	};
}

[[nodiscard]] static ThirdPartyHpackTestCase
get_thirdparty_hpack_test_case(const std::filesystem::path& path) {

	std::ifstream file_stream{ path };
	nlohmann::json data = nlohmann::json::parse(file_stream);

	const std::string description = data.at("description").get<std::string>();

	const std::string test_name = path.filename().stem();

	const std::string suite_name = path.parent_path().filename().string();

	std::vector<ThirdPartyHpackTestCaseEntry> cases{};

	if(!data.contains("cases") || !data["cases"].is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(size_t i = 0; i < data["cases"].size(); ++i) {
		const auto& case_ = data["cases"].at(i);

		const auto case_result = get_case_from_json(case_, suite_name, test_name);

		cases.push_back(case_result);
	}

	// cases post processing
	HeaderTableMode header_mode = { .all_the_same = true, .table_size = DEFAULT_HEADER_TABLE_SIZE };

	{
		std::optional<size_t> header_table_size = std::nullopt;

		for(size_t i = 0; i < cases.size(); ++i) {
			const auto& case_ = cases.at(i);

			const auto local_h_size = case_.header_table_size;

			if(local_h_size.has_value()) {
				if(header_table_size.has_value()) {
					if(local_h_size.value() != header_table_size.value()) {

						header_mode.all_the_same = false;
						break;
					}
				} else {
					header_table_size = local_h_size;
				}
			}
		}

		if(header_mode.all_the_same) {
			header_mode.table_size = header_table_size.value_or(DEFAULT_HEADER_TABLE_SIZE);
		} else {
			header_mode.table_size =
			    cases.size() == 0
			        ? DEFAULT_HEADER_TABLE_SIZE
			        : cases.at(0).header_table_size.value_or(DEFAULT_HEADER_TABLE_SIZE);
		}
	}

	return ThirdPartyHpackTestCase{
		.description = description,
		.cases = cases,
		.test_name = test_name,
		.header_mode = header_mode,
		.file = path,
	};
}

[[nodiscard]] static std::filesystem::path get_root_test_dir() {
	const std::filesystem::path root_tests_dir1 =
	    std::filesystem::current_path() / ".." / "thirdparty" / "hpack-test-case";

	if(std::filesystem::exists(root_tests_dir1)) {
		return root_tests_dir1;
	}

	const std::filesystem::path root_tests_dir2 =
	    std::filesystem::current_path() / "tests" / "unit" / "thirdparty" / "hpack-test-case";

	if(std::filesystem::exists(root_tests_dir2)) {
		return root_tests_dir2;
	}

	return root_tests_dir2;
}

[[nodiscard]] [[maybe_unused]] static std::vector<ThirdPartyHpackTestCase>
get_thirdparty_hpack_test_cases(const std::string& name) {

	const std::filesystem::path root_tests_dir = get_root_test_dir();

	if(!std::filesystem::exists(root_tests_dir)) {
		throw std::runtime_error("Invalid test launch from invalid cwd!");
	}

	const std::filesystem::path dir = (root_tests_dir / name).lexically_normal();

	if(!std::filesystem::exists(dir)) {
		throw std::runtime_error(std::string{ "Invalid test dir name: " } + dir.string());
	}

	if(!std::filesystem::is_directory(dir)) {
		throw std::runtime_error(std::string{ "Invalid test dir name: (no a dir)" } + dir.string());
	}

	std::vector<ThirdPartyHpackTestCase> result{};

	for(auto const& dir_entry : std::filesystem::directory_iterator{ dir }) {

		if(!dir_entry.is_regular_file()) {
			continue;
		}

		const auto value = get_thirdparty_hpack_test_case(dir_entry.path());

		result.push_back(value);
	}

	return result;
}

[[nodiscard]] static std::vector<std::pair<std::string, std::string>>
get_cpp_headers(const HttpHeaderFields& fields) {

	std::vector<std::pair<std::string, std::string>> result{};

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, fields, i);

		result.emplace_back(string_from_tstr(field.key), string_from_tstr(field.value));
	}

	return result;
}

[[maybe_unused]] [[nodiscard]] static CppDefer<HttpHeaderFields>
get_c_map_from_cpp(const std::vector<std::pair<std::string, std::string>>& map) {

	auto* result = (HttpHeaderFields*)malloc(sizeof(HttpHeaderFields));

	assert(result != NULL);

	*result = TVEC_EMPTY(HttpHeaderField);

	auto res = TVEC_RESERVE(HttpHeaderField, result, map.size());
	assert(res == TvecResultOk);

	for(const auto& elem : map) {
		HttpHeaderField value = { .key = tstr_from_string(elem.first),
			                      .value = tstr_from_string(elem.second) };

		res = TVEC_PUSH(HttpHeaderField, result, value);
		assert(res == TvecResultOk);
	}

	return CppDefer<HttpHeaderFields>{ result, [](HttpHeaderFields* const fields) -> void {
		                                  free_http_header_fields(fields);
		                                  free(fields);
		                              } };
}

using HpackDecompressStateCpp =
    std::unique_ptr<HpackDecompressState, void (*)(HpackDecompressState*)>;

[[nodiscard]] static HpackDecompressStateCpp
get_default_hpack_decompress_state_cpp(size_t max_dynamic_table_byte_size) {
	HpackDecompressStateCpp decompress_state{
		get_default_hpack_decompress_state(max_dynamic_table_byte_size), free_hpack_decompress_state
	};
	return decompress_state;
}

using HpackCompressStateCpp = std::unique_ptr<HpackCompressState, void (*)(HpackCompressState*)>;

[[maybe_unused]] [[nodiscard]] static HpackCompressStateCpp
get_default_hpack_compress_state_cpp(size_t max_dynamic_table_byte_size) {
	HpackCompressStateCpp compress_state{
		get_default_hpack_compress_state(max_dynamic_table_byte_size), free_hpack_compress_state
	};
	return compress_state;
}

[[maybe_unused]] static void free_hpack_decompress_result(Http2HpackDecompressResult* result) {

	if(result->is_error) {
		return;
	}

	free_http_header_fields(&(result->data.result));
}

// this is a hack, as I don't want to expose the state in the header for c, but i also want to test
// it, i am willing to copy paste this for the tests
namespace cpp_forbidden_test_type_impl_DONT_USE {

extern "C" {

#include "http/dynamic_hpack_table.h"

typedef struct {
	HpackHeaderDynamicTable dynamic_table;
	size_t max_dynamic_table_byte_size;
	size_t current_dynamic_table_byte_size;
} HpackDynamicTableState;

struct HpackDecompressStateImpl {
	HpackDynamicTableState dynamic_table_state;
};

struct HpackCompressStateImpl {
	HpackDynamicTableState dynamic_table_state;
};
}

} // namespace cpp_forbidden_test_type_impl_DONT_USE

namespace test {

struct DynamicTable {
	std::vector<std::pair<std::string, std::string>> entries;
	size_t size;
};

[[maybe_unused]] static std::ostream& operator<<(std::ostream& os,
                                                 const test::DynamicTable& table) {
	os << "DynamicTable:\n" << os_stream_formattable_to_doctest(table.entries);
	os << "\n" << table.size << "\n";
	return os;
}

NODISCARD [[maybe_unused]] static bool operator==(const DynamicTable& table1,
                                                  const DynamicTable& table2) {

	if(table1.size != table2.size) {
		return false;
	}

	const auto table1_vec = table1.entries;
	const auto table2_vec = table2.entries;

	if(table1_vec.size() != table2_vec.size()) {
		return false;
	}

	for(size_t i = 0; i < table1_vec.size(); ++i) {
		if(table1_vec.at(i) != table2_vec.at(i)) {
			return false;
		}
	}

	return true;
}

} // namespace test

[[nodiscard]] static test::DynamicTable get_dynamic_table(
    const cpp_forbidden_test_type_impl_DONT_USE::HpackDynamicTableState* const state) {

	const size_t size = state->current_dynamic_table_byte_size;

	std::vector<std::pair<std::string, std::string>> entries = {};

	for(size_t i = state->dynamic_table.start, rest_count = state->dynamic_table.count;
	    rest_count != 0; i = (i + 1) % state->dynamic_table.capacity, rest_count--) {
		const auto& entry = state->dynamic_table.entries[i];

		entries.emplace_back(string_from_tstr(entry.key), string_from_tstr(entry.value));
	}

	return test::DynamicTable{ .entries = entries, .size = size };
}

[[nodiscard]] static test::DynamicTable
get_dynamic_decompress_table(const HpackDecompressStateCpp& state) {

	const auto* state_cpp_extracted =
	    (const cpp_forbidden_test_type_impl_DONT_USE::HpackDecompressStateImpl*)state.get();

	return get_dynamic_table(&(state_cpp_extracted->dynamic_table_state));
}

template <typename T> struct OptionalOr {
  public:
	T value;
	OptionalOr(const T& val) : value{ val } {}

	friend std::ostream& operator<<(std::ostream& os, const OptionalOr<T>& val) {
		os << "OptionalOr{" << val.value << "}";
		return os;
	}

	[[nodiscard]] bool operator==(const std::optional<T>& lhs) const {
		if(!lhs.has_value()) {
			return true;
		}

		return this->value == lhs.value();
	}
};

namespace doctest {
template <typename T> struct StringMaker<OptionalOr<T>> {
	static String convert(const OptionalOr<T>& val) {
		return ::os_stream_formattable_to_doctest(val);
	}
};

} // namespace doctest
