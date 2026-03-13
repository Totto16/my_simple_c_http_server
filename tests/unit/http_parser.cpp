#include <doctest.h>
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

#include "../support/helpers.hpp"
#include "../support/helpers/http.hpp"

#include "./string_maker.hpp"

TEST_SUITE_BEGIN("http_parser" * doctest::description("http parser tests") * doctest::timeout(2.0));

TEST_CASE("testing parsing of the Accept-Encoding header <encoding_parser>") {

	REQUIRE(is_compression_supported(CompressionTypeGzip));
	REQUIRE(is_compression_supported(CompressionTypeDeflate));
	REQUIRE(is_compression_supported(CompressionTypeBr));
	REQUIRE(is_compression_supported(CompressionTypeZstd));
	REQUIRE(is_compression_supported(CompressionTypeCompress));

	REQUIRE(!is_compression_supported((CompressionType)(CompressionTypeCompress + 2)));

	SUBCASE("no Accept-Encoding header") {
		[]() -> void {
			HttpHeaderFields http_header_fields = TVEC_EMPTY(HttpHeaderField);

			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(http_header_fields);

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			REQUIRE_EQ(entries_length, 0);
		}();
	}

	SUBCASE("standard simple list") {
		[]() -> void {
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" compress, gzip");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			REQUIRE_EQ(entries_length, 2);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeCompress } },
				.weight = 1.0F
			};

			REQUIRE_EQ(entry1, entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeGzip } },
				.weight = 1.0F
			};

			REQUIRE_EQ(entry2, entry2Expected);
		}();
	}

	SUBCASE("empty value") {
		[]() -> void {
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp("");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			REQUIRE_EQ(entries_length, 0);
		}();
	}

	SUBCASE("'*' value") {
		[]() -> void {
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" *");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			REQUIRE_EQ(entries_length, 1);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 1.0F
			};

			REQUIRE_EQ(entry1, entry1Expected);
		}();
	}

	SUBCASE("complicated list with weights") {
		[]() -> void {
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" deflate;q=0.5, br;q=1.0");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			REQUIRE_EQ(entries_length, 2);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeDeflate } },
				.weight = 0.5F
			};

			REQUIRE_EQ(entry1, entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeBr } },
				.weight = 1.0F
			};

			REQUIRE_EQ(entry2, entry2Expected);
		}();
	}

	SUBCASE("complicated list with weights and 'identity'") {
		[]() -> void {
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" zstd;q=1.0, identity; q=0.5, *;q=0");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			REQUIRE_EQ(entries_length, 3);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeZstd } },
				.weight = 1.0F
			};

			REQUIRE_EQ(entry1, entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNoEncoding, .data = {} }, .weight = 0.5F
			};

			REQUIRE_EQ(entry2, entry2Expected);

			CompressionEntry entry3 = TVEC_AT(CompressionEntry, compression_settings.entries(), 2);

			CompressionEntry entry3Expected = {
				.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 0.0F
			};

			REQUIRE_EQ(entry3, entry3Expected);
		}();
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

TEST_CASE("testing the parsing of the http request - test url path parsing <url_parser>") {

	SUBCASE("simple url") {
		[]() -> void {
			auto parsed_path = parse_uri("/");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			REQUIRE_EQ(path_comp, "/");

			REQUIRE_EQ(TMAP_SIZE(ParsedSearchPathHashMap, &path.search_path.hash_map), 0);
		}();
	}

	SUBCASE("real path url") {
		[]() -> void {
			auto parsed_path = parse_uri("/test/hello");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			REQUIRE_EQ(path_comp, "/test/hello");

			REQUIRE_EQ(TMAP_SIZE(ParsedSearchPathHashMap, &path.search_path.hash_map), 0);
		}();
	}

	SUBCASE("path url with search parameters") {
		[]() -> void {
			auto parsed_path = parse_uri("/test/hello?param1=hello&param2&param3=");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			REQUIRE_EQ(path_comp, "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			REQUIRE_EQ(TMAP_SIZE(ParsedSearchPathHashMap, &search_path.hash_map), 3);

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param1"_tstr);

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(string_from_tstr(entry->key), "param1");

				REQUIRE_EQ(string_from_tstr(entry->value.val), "hello");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param2"_tstr);

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(string_from_tstr(entry->key), "param2");

				REQUIRE_EQ(string_from_tstr(entry->value.val), "");
			}

			{
				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param3"_tstr);

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(string_from_tstr(entry->key), "param3");

				REQUIRE_EQ(string_from_tstr(entry->value.val), "");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param4"_tstr);

				REQUIRE_EQ(entry, nullptr);
			}
		}();
	}

	SUBCASE("path url with search parameters") {
		[]() -> void {
			auto parsed_path = parse_uri("/test/hello?param1=hello&param2&param3=");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			REQUIRE_EQ(path_comp, "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			REQUIRE_EQ(TMAP_SIZE(ParsedSearchPathHashMap, &search_path.hash_map), 3);

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param1"_tstr);

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(string_from_tstr(entry->key), "param1");

				REQUIRE_EQ(string_from_tstr(entry->value.val), "hello");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param2"_tstr);

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(string_from_tstr(entry->key), "param2");

				REQUIRE_EQ(string_from_tstr(entry->value.val), "");
			}

			{
				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param3"_tstr);

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(string_from_tstr(entry->key), "param3");

				REQUIRE_EQ(string_from_tstr(entry->value.val), "");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param4"_tstr);

				REQUIRE_EQ(entry, nullptr);
			}
		}();
	}
}

TEST_SUITE_END();
