
#include "./hpack.hpp"

#include "../helpers.hpp"

#include <atomic>

#define TEST_ENV_PREFIX "TOTTO_SIMPLE_HTTP_SERVER___ENV___HACK_IMPL"

static const char* GLOBAL_CALLBACK_PLACEHOLDER =
    "NOT SETUP PROPERLY, you need to use HpackDecodingErrorStateHack to call this function "
    "in test mode";

hpack::HpackGlobalHandle::HpackGlobalHandle() {
	global_initialize_http2_hpack_data();

	setenv(TEST_ENV_PREFIX "_CALLBACK_FN", GLOBAL_CALLBACK_PLACEHOLDER, 1);
}

hpack::HpackGlobalHandle::~HpackGlobalHandle() noexcept(false) {
	global_free_http2_hpack_data();

	const char* cb = getenv(TEST_ENV_PREFIX "_CALLBACK_FN");

	if(cb != NULL) {
		if(strcmp(cb, GLOBAL_CALLBACK_PLACEHOLDER) != 0) {
			// we didn't run the cleanup correctly
			throw std::runtime_error("The test CB env variable wasn't cleaned up correctly!");
		}
	}
}

typedef void (*CbFn)(const tstr* const str);

void setup_global_env_for_hack();

struct HpackGlobalTrickData {
	std::atomic<bool> present;
	std::vector<std::string> errors;

	void setup() {
		bool expected = false;
		const auto res = this->present.compare_exchange_strong(expected, true);

		if(!res) {
			throw std::runtime_error(
			    "only one HpackDecodingErrorStateHack struct can be used at a time!");
		}

		this->errors = {};

		setup_global_env_for_hack();
	}

	void destroy() {
		bool expected = true;
		const auto res = this->present.compare_exchange_strong(expected, false);

		if(!res) {
			throw std::runtime_error("error in HpackDecodingErrorStateHack destructor");
		}

		this->errors = {};
	}

	void add_error(const tstr* const str) { this->errors.push_back(string_from_tstr(*str)); }
};

static HpackGlobalTrickData g_hack_trick_variable = { .present = false, .errors = {} };

static void g_hack_trick_add_error(const tstr* const str) {
	g_hack_trick_variable.errors.push_back(string_from_tstr(*str));
}

void setup_global_env_for_hack() {
	void* cb_fn = malloc(sizeof(uint8_t) + sizeof(void*) + 1);

	if(cb_fn == NULL) {
		throw std::runtime_error("OOM");
	}

	// NOTE: cb_fn stores the function ptr and a mask, to specify not 0 bytes, so that the
	// strlen(returns the correct thing, we patch the ptr in places where it has a 0 and set the
	// bit of that position to 0, so that even if 7 bytes are 0 one has to be at least non zero
	// to be a valid ptr, the byte nmask is also always non zero guaranteed)

	{
		uint8_t* const byte_mask_ptr = (uint8_t*)((uint8_t*)cb_fn + sizeof(void*));

		static_assert(sizeof(void*) <= (sizeof(uint8_t) * 8));
		*byte_mask_ptr = 0xFF;

		// set 0 terminator
		*(((uint8_t*)cb_fn) + (sizeof(uint8_t) + sizeof(void*))) = 0x00;

		{
			CbFn fn_ptr = &g_hack_trick_add_error;

			memcpy(cb_fn, (void*)&fn_ptr, sizeof(void*));
		}

		{
			uint8_t* const fn_values = (uint8_t*)cb_fn;

			for(size_t i = 0; i < sizeof(void*); ++i) {
				const uint8_t val = fn_values[i];

				if(val == 0) {
					*byte_mask_ptr = *byte_mask_ptr & (~(1 << i));
					fn_values[i] = 0xFF;
				}
			}
		}
	}

	const size_t cb_len = strlen((char*)cb_fn);

	if(cb_len != (sizeof(void*) + sizeof(uint8_t))) {
		throw std::runtime_error(std::string{ "invalid encoding of the ptr: size is " } +
		                         std::to_string(cb_len) + " but not " +
		                         std::to_string((sizeof(void*) + sizeof(uint8_t))));
	}

	setenv(TEST_ENV_PREFIX "_CALLBACK_FN", (char*)cb_fn, 1);
}

hpack::hacky_trick::HpackDecodingErrorStateHack::HpackDecodingErrorStateHack() {

#ifdef NDEBUG
	#error \
	    "this class doesn't work in non debug mode, as than the library doesn't use this expensive global tracking"
#endif

	g_hack_trick_variable.setup();
}

hpack::hacky_trick::HpackDecodingErrorStateHack::~HpackDecodingErrorStateHack() noexcept(false) {
	g_hack_trick_variable.destroy();
	unsetenv(TEST_ENV_PREFIX "_CALLBACK_FN");
}

[[nodiscard]] std::vector<std::string>
hpack::hacky_trick::HpackDecodingErrorStateHack::get_errors() const {
	return g_hack_trick_variable.errors;
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

std::vector<consts::StrictErrorException> helpers::get_strict_error_state_exceptions() {

	return {
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
		// searched for by using "636f6e74656e742d7479706500" alias "content-type" in ascii and than
		// 00
		consts::StrictErrorException{
		    .suite_name = "haskell-http2-naive",
		    .test_name = "story_30",
		    .seqno = 138,
		    .field_name = "content-type",
		},

		// manually checked, is a valid field encoded with a string literal
		// (0x00) alias a 0 long string literal, the whole string uses 0x00 bytes at the start alias
		// always everything is encoded as "Literal Header Field without Indexing",
		// searched for by using "636f6e74656e742d7479706500" alias "content-type" in ascii and than
		// 00
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
};

const size_t consts::default_header_table_size = 4096;

[[nodiscard]] std::vector<std::pair<std::string, std::string>>
helpers::get_cpp_headers(const HttpHeaderFields& fields) {

	std::vector<std::pair<std::string, std::string>> result{};

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, fields, i);

		result.emplace_back(string_from_tstr(field.key), string_from_tstr(field.value));
	}

	return result;
}

[[nodiscard]] CAutoFreePtr<HttpHeaderFields>
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

	return CAutoFreePtr<HttpHeaderFields>{ result, [](HttpHeaderFields* const fields) -> void {
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
