
#include "./hpack.hpp"

#define TEST_ENV_PREFIX "TOTTO_SIMPLE_HTTP_SERVER___ENV___HACK_IMPL"

static void test_framework_setup_global_data() {
	// TODO: remove this after hpack decompress doesn't use this anymore!
	setenv(TEST_ENV_PREFIX "_IS_TEST", "TRUE", 1);
	setenv(TEST_ENV_PREFIX "_STRICT_QUIET", "FALSE", 1);
}

static void test_framework_free_global_data() {

	unsetenv(TEST_ENV_PREFIX "_IS_TEST");
	unsetenv(TEST_ENV_PREFIX "_STRICT_QUIET");
}

hpack::HpackGlobalHandle::HpackGlobalHandle() {

	global_initialize_http2_hpack_data();
	test_framework_setup_global_data();
}

hpack::HpackGlobalHandle::~HpackGlobalHandle() {
	global_free_http2_hpack_data();
	test_framework_free_global_data();
}

// TODO: use callback function inside set env variable,as this can anyway onle be one static
// instance! store 0 byte with 0 byte map, as one byte has to be non null, store if byte is nonnull,
// modify null bytes and store

hpack::hacky_trick::HpackDecodingErrorStateHack::HpackDecodingErrorStateHack() {

#ifdef NDEBUG
	#error \
	    "this class doesn't work in non debug mode, as than the library doesn't use this expensive global tracking"
#endif

	// set quiet, as we collect the result
	setenv(TEST_ENV_PREFIX "_STRICT_QUIET", "TRUE", 1);

	setenv(TEST_ENV_PREFIX "_TEST_CASE_FAILED_KEY_NAMES", "", 1);
}

hpack::hacky_trick::HpackDecodingErrorStateHack::~HpackDecodingErrorStateHack() {
	setenv(TEST_ENV_PREFIX "_STRICT_QUIET", "FALSE", 1);
	unsetenv(TEST_ENV_PREFIX "_TEST_CASE_FAILED_KEY_NAMES");
}

[[nodiscard]] std::vector<std::string>
hpack::hacky_trick::HpackDecodingErrorStateHack::get_errors() const {

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

[[nodiscard]] std::vector<std::uint8_t>
hpack::helpers::parse_wire_data(const std::string& raw_wire) {

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

[[nodiscard]] bool consts::StrictErrorException::operator==(const StrictErrorException& lhs) const {
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

std::vector<consts::StrictErrorException> strict_error_state_exceptions = {
	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal
	consts::StrictErrorException{
	    .suite_name = "nghttp2-change-table-size",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing"
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing"
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing",
	// searched for by using "636f6e74656e742d7479706500" alias "content-type" in ascii and than 00
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_30",
	    .seqno = 138,
	    .field_name = "content-type",
	},

	// manually checked, is a valid field encoded with a string literal
	// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
	// always everything is encoded as "Literal Header Field without Indexing",
	// searched for by using "636f6e74656e742d7479706500" alias "content-type" in ascii and than 00
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-naive",
	    .test_name = "story_30",
	    .seqno = 599,
	    .field_name = "content-type",
	},

	// TODO: investigate
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-linear-huffman",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	consts::StrictErrorException{
	    .suite_name = "python-hpack",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-linear",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	consts::StrictErrorException{
	    .suite_name = "go-hpack",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate
	consts::StrictErrorException{
	    .suite_name = "haskell-http2-static-huffman",
	    .test_name = "story_25",
	    .seqno = 0,
	    .field_name = "etag",
	},

	// TODO: investigate, likely legit!
	consts::StrictErrorException{
	    .suite_name = "nghttp2-16384-4096",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// TODO: investigate, likely legit!
	consts::StrictErrorException{
	    .suite_name = "node-http2-hpack",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	},

	// TODO: investigate, likely legit!
	consts::StrictErrorException{
	    .suite_name = "nghttp2",
	    .test_name = "story_23",
	    .seqno = 243,
	    .field_name = "pragma",
	}

};

const size_t consts::default_header_table_size = 4096;

[[nodiscard]] static tests::ThirdPartyHpackTestCaseEntry
get_case_from_json(const nlohmann::json& value, const std::string& suite_name,
                   const std::string& test_name) {

	size_t seqno = value["seqno"].get<size_t>();

	const std::string raw_wire_data = value["wire"].get<std::string>();

	const auto wire_data = hpack::helpers::parse_wire_data(raw_wire_data);

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

	return tests::ThirdPartyHpackTestCaseEntry{
		.seqno = seqno,
		.wire_data = wire_data,
		.headers = headers,
		.header_table_size = header_table_size.value_or(consts::default_header_table_size),
		.strict_error_state = strict_error_state,
	};
}

[[nodiscard]] static tests::ThirdPartyHpackTestCase
get_thirdparty_hpack_test_case(const std::filesystem::path& path) {

	std::ifstream file_stream{ path };
	nlohmann::json data = nlohmann::json::parse(file_stream);

	const std::string description = data.at("description").get<std::string>();

	const std::string test_name = path.filename().stem();

	const std::string suite_name = path.parent_path().filename().string();

	std::vector<tests::ThirdPartyHpackTestCaseEntry> cases{};

	if(!data.contains("cases") || !data["cases"].is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(size_t i = 0; i < data["cases"].size(); ++i) {
		const auto& case_ = data["cases"].at(i);

		const auto case_result = get_case_from_json(case_, suite_name, test_name);

		cases.push_back(case_result);
	}

	// cases post processing
	tests::HeaderTableMode header_mode = { .all_the_same = true,
		                                   .table_size = consts::default_header_table_size };

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
			header_mode.table_size = header_table_size.value_or(consts::default_header_table_size);
		} else {
			header_mode.table_size =
			    cases.size() == 0
			        ? consts::default_header_table_size
			        : cases.at(0).header_table_size.value_or(consts::default_header_table_size);
		}
	}

	return tests::ThirdPartyHpackTestCase{
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

[[nodiscard]] std::vector<tests::ThirdPartyHpackTestCase>
hpack::get_thirdparty_hpack_test_cases(const std::string& name) {

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

	std::vector<tests::ThirdPartyHpackTestCase> result{};

	for(auto const& dir_entry : std::filesystem::directory_iterator{ dir }) {

		if(!dir_entry.is_regular_file()) {
			continue;
		}

		const auto value = get_thirdparty_hpack_test_case(dir_entry.path());

		result.push_back(value);
	}

	return result;
}

[[nodiscard]] std::vector<std::pair<std::string, std::string>>
helpers::get_cpp_headers(const HttpHeaderFields& fields) {

	std::vector<std::pair<std::string, std::string>> result{};

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, fields, i);

		result.emplace_back(string_from_tstr(field.key), string_from_tstr(field.value));
	}

	return result;
}

[[nodiscard]] CppDefer<HttpHeaderFields>
helpers::get_c_map_from_cpp(const std::vector<std::pair<std::string, std::string>>& map) {

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

[[nodiscard]] hpack::HpackDecompressStateCpp
hpack::get_default_hpack_decompress_state_cpp(size_t max_dynamic_table_byte_size) {
	HpackDecompressStateCpp decompress_state{
		get_default_hpack_decompress_state(max_dynamic_table_byte_size), free_hpack_decompress_state
	};
	return decompress_state;
}

[[nodiscard]] hpack::HpackCompressStateCpp
hpack::get_default_hpack_compress_state_cpp(size_t max_dynamic_table_byte_size) {
	HpackCompressStateCpp compress_state{
		get_default_hpack_compress_state(max_dynamic_table_byte_size), free_hpack_compress_state
	};
	return compress_state;
}

void hpack::free_hpack_decompress_result(Http2HpackDecompressResult* result) {

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

std::ostream& operator<<(std::ostream& os, const test::DynamicTable& table) {
	os << "DynamicTable:\n" << table.entries;
	os << "\n" << table.size << "\n";
	return os;
}

[[nodiscard]] bool test::DynamicTable::operator==(const DynamicTable& table2) const {

	if(this->size != table2.size) {
		return false;
	}

	const auto table1_vec = this->entries;
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

[[nodiscard]] test::DynamicTable
hpack::get_dynamic_decompress_table(const HpackDecompressStateCpp& state) {

	const auto* state_cpp_extracted =
	    (const cpp_forbidden_test_type_impl_DONT_USE::HpackDecompressStateImpl*)state.get();

	return get_dynamic_table(&(state_cpp_extracted->dynamic_table_state));
}

[[nodiscard]] test::DynamicTable
hpack::get_dynamic_compress_table(const HpackCompressStateCpp& state) {

	const auto* state_cpp_extracted =
	    (const cpp_forbidden_test_type_impl_DONT_USE::HpackCompressStateImpl*)state.get();

	return get_dynamic_table(&(state_cpp_extracted->dynamic_table_state));
}

helpers::GlobalHuffmanData::GlobalHuffmanData() : present{ true } {
	global_initialize_http2_hpack_huffman_data();
}

helpers::GlobalHuffmanData::~GlobalHuffmanData() {
	global_free_http2_hpack_huffman_data();
}

void helpers::free_huffman_decode_result(HuffmanDecodeResult* decode_result) {
	if(decode_result->is_error) {
		return;
	}

	free_sized_buffer(decode_result->data.result);
}

void helpers::free_huffman_encode_result(HuffmanEncodeResult* encode_result) {
	if(encode_result->is_error) {
		return;
	}

	free_sized_buffer(encode_result->data.result);
}

[[nodiscard]] std::vector<std::uint8_t> hpack::huffman::all_values_vector() {
	std::vector<std::uint8_t> result = {};
	result.reserve(256);

	for(size_t i = 0; i < 256; ++i) {
		result.emplace_back((uint8_t)i);
	}

	return result;
}
