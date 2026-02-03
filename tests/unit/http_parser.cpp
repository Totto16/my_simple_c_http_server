#include <doctest.h>
#include <zmap/zmap.h>
#include <zvec/zvec.h>

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

	CompressionSettingsCpp(const char* accept_encoding_value) {

		HttpHeaderFields http_header_fields = ZVEC_EMPTY(HttpHeaderField);

		char* accept_encoding_buffer = NULL;
		FORMAT_STRING_IMPL(&accept_encoding_buffer, throw std::runtime_error("OOM");
		                   , IMPL_STDERR_LOGGER, "%s%c%s", HTTP_HEADER_NAME(accept_encoding), '\0',
		                   accept_encoding_value);

		add_http_header_field_by_double_str(&http_header_fields, accept_encoding_buffer);

		this->m_settings = get_compression_settings(http_header_fields);

		free_http_header_fields(&http_header_fields);
	}

	[[nodiscard]] const CompressionEntries& entries() const { return this->m_settings.entries; }

	~CompressionSettingsCpp() { free_compression_settings(this->m_settings); }
};

[[nodiscard]] const char* compression_type_to_string(CompressionType type) {
	return get_string_for_compress_format(type);
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

std::ostream& operator<<(std::ostream& os, const CompressionEntry& entry) {
	os << "CompressionEntry{value=" << get_representation_for_compression_value(entry.value)
	   << ", weight=" << entry.weight << "}";
	return os;
}

} // namespace

doctest::String static toString(const CompressionEntry& value) {
	std::stringstream str{};
	str << value;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

[[nodiscard]] static bool operator==(const CompressionEntry& lhs, const CompressionEntry& rhs) {

	if(lhs.value != rhs.value) {
		return false;
	}

	return lhs.weight == rhs.weight;
}

TEST_CASE("testing parsing of the Accept-Encoding header") {

	REQUIRE(is_compression_supported(CompressionTypeGzip));
	REQUIRE(is_compression_supported(CompressionTypeDeflate));
	REQUIRE(is_compression_supported(CompressionTypeBr));
	REQUIRE(is_compression_supported(CompressionTypeZstd));
	REQUIRE(is_compression_supported(CompressionTypeCompress));

	REQUIRE(!is_compression_supported((CompressionType)(CompressionTypeCompress + 2)));

	SUBCASE("no Accept-Encoding header") {

		HttpHeaderFields http_header_fields = ZVEC_EMPTY(HttpHeaderField);

		CompressionSettingsCpp compression_settings = CompressionSettingsCpp(http_header_fields);

		size_t entries_length = ZVEC_LENGTH(compression_settings.entries());

		REQUIRE_EQ(entries_length, 0);
	}

	SUBCASE("standard simple list") {

		CompressionSettingsCpp compression_settings = CompressionSettingsCpp(" compress, gzip");

		size_t entries_length = ZVEC_LENGTH(compression_settings.entries());

		REQUIRE_EQ(entries_length, 2);

		CompressionEntry entry1 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 0);

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeCompress } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 1);

		CompressionEntry entry2Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeGzip } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry2, entry2Expected);
	}

	SUBCASE("empty value") {

		CompressionSettingsCpp compression_settings = CompressionSettingsCpp("");

		size_t entries_length = ZVEC_LENGTH(compression_settings.entries());

		REQUIRE_EQ(entries_length, 0);
	}

	SUBCASE("'*' value") {

		CompressionSettingsCpp compression_settings = CompressionSettingsCpp(" *");

		size_t entries_length = ZVEC_LENGTH(compression_settings.entries());

		REQUIRE_EQ(entries_length, 1);

		CompressionEntry entry1 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 0);

		CompressionEntry entry1Expected = {
			.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 1.0F
		};

		REQUIRE_EQ(entry1, entry1Expected);
	}

	SUBCASE("complicated list with weights") {
		CompressionSettingsCpp compression_settings =
		    CompressionSettingsCpp(" deflate;q=0.5, br;q=1.0");

		size_t entries_length = ZVEC_LENGTH(compression_settings.entries());

		REQUIRE_EQ(entries_length, 2);

		CompressionEntry entry1 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 0);

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeDeflate } },
			                                .weight = 0.5F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 1);

		CompressionEntry entry2Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeBr } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry2, entry2Expected);
	}

	SUBCASE("complicated list with weights and 'identity'") {
		CompressionSettingsCpp compression_settings =
		    CompressionSettingsCpp(" zstd;q=1.0, identity; q=0.5, *;q=0");

		size_t entries_length = ZVEC_LENGTH(compression_settings.entries());

		REQUIRE_EQ(entries_length, 3);

		CompressionEntry entry1 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 0);

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeZstd } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 1);

		CompressionEntry entry2Expected = {
			.value = { .type = CompressionValueTypeNoEncoding, .data = {} }, .weight = 0.5F
		};

		REQUIRE_EQ(entry2, entry2Expected);

		CompressionEntry entry3 = ZVEC_AT(CompressionEntry, compression_settings.entries(), 2);

		CompressionEntry entry3Expected = {
			.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 0.0F
		};

		REQUIRE_EQ(entry3, entry3Expected);
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
};

ParsedURIWrapper parse_uri(const char* value) {

	std::string readable_copy = std::string{ value };

	auto result = parse_request_uri(readable_copy.data());

	return ParsedURIWrapper(result);
}

} // namespace

TEST_CASE("testing the parsing of the http request") {

	SUBCASE("test url path parsing") {

		SUBCASE("simple url") {

			auto parsed_path = parse_uri("/");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/");

			REQUIRE_EQ(ZMAP_SIZE(path.search_path.hash_map), 0);
		}

		SUBCASE("real path url") {

			auto parsed_path = parse_uri("/test/hello");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/test/hello");

			REQUIRE_EQ(ZMAP_SIZE(path.search_path.hash_map), 0);
		}

		SUBCASE("path url with search parameters") {

			auto parsed_path = parse_uri("/test/hello?param1=hello&param2&param3=");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			REQUIRE_EQ(ZMAP_SIZE(search_path.hash_map), 3);

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param1");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param1");

				REQUIRE_EQ(std::string{ entry->value.value }, "hello");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param2");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param2");

				REQUIRE_EQ(std::string{ entry->value.value }, "");
			}

			{
				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param3");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param3");

				REQUIRE_EQ(std::string{ entry->value.value }, "");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param4");

				REQUIRE_EQ(entry, nullptr);
			}
		}

		SUBCASE("path url with search parameters") {

			auto parsed_path = parse_uri("/test/hello?param1=hello&param2&param3=");

			REQUIRE_EQ(parsed_path.error(), nullptr);

			const auto& path = parsed_path.path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			REQUIRE_EQ(ZMAP_SIZE(search_path.hash_map), 3);

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param1");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param1");

				REQUIRE_EQ(std::string{ entry->value.value }, "hello");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param2");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param2");

				REQUIRE_EQ(std::string{ entry->value.value }, "");
			}

			{
				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param3");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param3");

				REQUIRE_EQ(std::string{ entry->value.value }, "");
			}

			{

				const ParsedSearchPathEntry* entry = find_search_key(search_path, "param4");

				REQUIRE_EQ(entry, nullptr);
			}
		}
	}
}
