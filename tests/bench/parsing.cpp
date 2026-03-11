
#include <benchmark/benchmark.h>

#include <tmap.h>
#include <tvec.h>

#include <http/compression.h>
#include <http/header.h>
#include <http/parser.h>
#include <http/protocol.h>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace {
class CompressionSettingsCpp {
  private:
	CompressionSettings m_settings;

  public:
	CompressionSettingsCpp(HttpHeaderFields http_header_fields)
	    : m_settings{ get_compression_settings(http_header_fields) } {}

	CompressionSettingsCpp(CompressionSettings settings) : m_settings{ settings } {}

	CompressionSettingsCpp(const char* const accept_encoding_value) {

		HttpHeaderFields http_header_fields = TVEC_EMPTY(HttpHeaderField);

		add_http_header_field(&http_header_fields, HTTP_HEADER_NAME(accept_encoding),
		                      tstr_from(accept_encoding_value));

		this->m_settings = get_compression_settings(http_header_fields);

		free_http_header_fields(&http_header_fields);
	}

	[[nodiscard]] const CompressionEntries& entries() const { return this->m_settings.entries; }

	CompressionSettingsCpp(CompressionSettingsCpp&&) = delete;

	CompressionSettingsCpp(const CompressionSettingsCpp&) = delete;

	CompressionSettingsCpp& operator=(const CompressionSettingsCpp&) = delete;

	CompressionSettingsCpp operator=(CompressionSettingsCpp&&) = delete;

	~CompressionSettingsCpp() { free_compression_settings(this->m_settings); }
};

[[nodiscard]] const char* compression_type_to_string(CompressionType type) {
	const tstr temp = get_string_for_compress_format(type);
	return tstr_cstr(&temp);
}

[[nodiscard]] const char* get_representation_for_compression_value(CompressionValue value) {
	switch(value.type) {
		case CompressionValueTypeNoEncoding: return "'identity'";
		case CompressionValueTypeAllEncodings: return "'*'";
		case CompressionValueTypeNormalEncoding: {
			return compression_type_to_string(value.data.normal_compression);
		}
		default: {
			UNREACHABLE();
		}
	}
}

[[nodiscard]] bool operator==(const CompressionValue& lhs, const CompressionValue& rhs) {

	if(lhs.type != rhs.type) {
		return false;
	}

	if(lhs.type == CompressionValueTypeNormalEncoding) {
		return lhs.data.normal_compression == rhs.data.normal_compression;
	}

	return true;
}

} // namespace

[[maybe_unused]] static std::ostream& operator<<(std::ostream& os, const CompressionEntry& entry) {
	os << "CompressionEntry{value=" << get_representation_for_compression_value(entry.value)
	   << ", weight=" << entry.weight << "}";
	return os;
}

[[nodiscard]] static bool operator==(const CompressionEntry& lhs, const CompressionEntry& rhs) {

	if(lhs.value != rhs.value) {
		return false;
	}

	return lhs.weight == rhs.weight;
}
static void BM_encoding_parser(benchmark::State& state) {

	assert(is_compression_supported(CompressionTypeGzip));
	assert(is_compression_supported(CompressionTypeDeflate));
	assert(is_compression_supported(CompressionTypeBr));
	assert(is_compression_supported(CompressionTypeZstd));
	assert(is_compression_supported(CompressionTypeCompress));

	assert(!is_compression_supported((CompressionType)(CompressionTypeCompress + 2)));

	for(auto _ : state) {

		// SUBCASE("no Accept-Encoding header")
		{

			HttpHeaderFields http_header_fields = TVEC_EMPTY(HttpHeaderField);

			CompressionSettingsCpp compression_settings =
			    CompressionSettingsCpp(http_header_fields);

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 0);
		}

		// SUBCASE("standard simple list")
		{

			CompressionSettingsCpp compression_settings = CompressionSettingsCpp(" compress, gzip");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 2);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeCompress } },
				.weight = 1.0F
			};

			assert(entry1 == entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeGzip } },
				.weight = 1.0F
			};

			assert(entry2 == entry2Expected);
		}

		// SUBCASE("empty value")
		{

			CompressionSettingsCpp compression_settings = CompressionSettingsCpp("");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 0);
		}

		// SUBCASE("'*' value")
		{

			CompressionSettingsCpp compression_settings = CompressionSettingsCpp(" *");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 1);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 1.0F
			};

			assert(entry1 == entry1Expected);
		}

		// SUBCASE("complicated list with weights")
		{
			CompressionSettingsCpp compression_settings =
			    CompressionSettingsCpp(" deflate;q=0.5, br;q=1.0");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 2);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeDeflate } },
				.weight = 0.5F
			};

			assert(entry1 == entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeBr } },
				.weight = 1.0F
			};

			assert(entry2 == entry2Expected);
		}

		// SUBCASE("complicated list with weights and 'identity'")
		{
			CompressionSettingsCpp compression_settings =
			    CompressionSettingsCpp(" zstd;q=1.0, identity; q=0.5, *;q=0");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 3);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeZstd } },
				.weight = 1.0F
			};

			assert(entry1 == entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNoEncoding, .data = {} }, .weight = 0.5F
			};

			assert(entry2 == entry2Expected);

			CompressionEntry entry3 = TVEC_AT(CompressionEntry, compression_settings.entries(), 2);

			CompressionEntry entry3Expected = {
				.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 0.0F
			};

			assert(entry3 == entry3Expected);
		}
	}
}

namespace {

struct ParsedURIWrapper {
  private:
	ParsedRequestURIResult m_result;

  public:
	ParsedURIWrapper(ParsedRequestURIResult result) : m_result{ result } {}

	[[nodiscard]] const ParsedURLPath& path() const {
		if(m_result.is_error) {
			throw std::runtime_error("invalid parse url result: " +
			                         std::string{ m_result.value.error });
		}

		switch(m_result.value.uri.type) {
			case ParsedURITypeAbsoluteURI: {
				return m_result.value.uri.data.uri.path;
			};
			case ParsedURITypeAbsPath: {
				return m_result.value.uri.data.path;
			}
			default: {
				throw std::runtime_error("invalid parse url result: " +
				                         std::to_string(m_result.value.uri.type));
			}
		}
	}

	[[nodiscard]] const char* error() const {
		if(m_result.is_error) {
			return m_result.value.error;
		}

		return NULL;
	}

	ParsedURIWrapper(ParsedURIWrapper&&) = delete;

	ParsedURIWrapper(const ParsedURIWrapper&) = delete;

	ParsedURIWrapper& operator=(const ParsedURIWrapper&) = delete;

	ParsedURIWrapper operator=(ParsedURIWrapper&&) = delete;

	~ParsedURIWrapper() {
		if(m_result.is_error) {
			return;
		}

		free_parsed_request_uri(this->m_result.value.uri);
	}
};

ParsedURIWrapper parse_uri(const std::string& uri) {

	auto result = parse_request_uri(tstr_view{ uri.data(), uri.size() });

	return ParsedURIWrapper(result);
}

} // namespace

[[nodiscard]] static inline std::string string_from_tstr(const tstr& value) {
	return std::string{ tstr_cstr(&value), tstr_len(&value) };
}

[[nodiscard]] static inline tstr operator""_tstr(const char* str, std::size_t len) {
	return tstr_from_static_cstr_with_len(str, len);
}

static void BM_url_parser(benchmark::State& state) {

	for(auto _ : state) {

		// SUBCASE("simple url")
		{

			auto parsed_path = parse_uri("/");

			assert(parsed_path.error() == nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/");

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &path.search_path.hash_map) == 0);
		}

		//	SUBCASE("real path url")
		{

			auto parsed_path = parse_uri("/test/hello");

			assert(parsed_path.error() == nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/test/hello");

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &path.search_path.hash_map) == 0);
		}

		// SUBCASE("path url with search parameters")
		{

			auto parsed_path = parse_uri("/test/hello?param1=hello&param2&param3=");

			assert(parsed_path.error() == nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &search_path.hash_map) == 3);

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param1"_tstr);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param1");

				assert(string_from_tstr(entry->value.val) == "hello");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param2"_tstr);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param2");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{
				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param3"_tstr);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param3");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param4"_tstr);

				assert(entry == nullptr);
			}
		}

		// SUBCASE("path url with search parameters")
		{

			auto parsed_path = parse_uri("/test/hello?param1=hello&param2&param3=");

			assert(parsed_path.error() == nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &search_path.hash_map) == 3);

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param1"_tstr);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param1");

				assert(string_from_tstr(entry->value.val) == "hello");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param2"_tstr);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param2");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{
				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param3"_tstr);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param3");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param4"_tstr);

				assert(entry == nullptr);
			}
		}
	}
}

BENCHMARK(BM_encoding_parser)->Name("parse/encoding");

BENCHMARK(BM_url_parser)->Name("parse/url");
