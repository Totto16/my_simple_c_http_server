

#include <doctest.h>
#include <stb/ds.h>

#include <http/compression.h>
#include <http/header.h>
#include <http/http_protocol.h>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace {
using CompressionSettingsCppPtr =
    std::unique_ptr<CompressionSettings, void (*)(CompressionSettings*)>;

[[nodiscard]] CompressionSettingsCppPtr
get_compression_settings_cpp(HttpHeaderFields http_header_fields) {

	CompressionSettings* compression_settings = get_compression_settings(http_header_fields);

	return { compression_settings, free_compression_settings };
}

[[nodiscard]] CompressionSettingsCppPtr
get_compression_setting_by_accept_encoding_header(const char* accept_encoding_value) {
	HttpHeaderFields http_header_fields = STBDS_ARRAY_EMPTY;

	char* accept_encoding_buffer = NULL;
	FORMAT_STRING_IMPL(&accept_encoding_buffer, throw std::runtime_error("OOM");
	                   , IMPL_STDERR_LOGGER, "%s%c%s", g_header_accept_encoding, '\0',
	                   accept_encoding_value);

	add_http_header_field_by_double_str(&http_header_fields, accept_encoding_buffer);

	CompressionSettingsCppPtr compression_settings =
	    get_compression_settings_cpp(http_header_fields);

	free_http_header_fields(&http_header_fields);

	return compression_settings;
}

[[nodiscard]] const char* compression_type_to_string(CompressionType type) {
	return get_string_for_compress_format(type);
}

[[nodiscard]] const char* get_representation_for_compression_value(CompressionValue value) {
	switch(value.type) {
		case CompressionValueTypeNoEncoding: return "'identity'";
		case CompressionValueTypeAllEncodings: return "'*'";
		case CompressionValueTypeNormalEncoding:
			return compression_type_to_string(value.data.normal_compression);
		default: UNREACHABLE();
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

doctest::String toString(const CompressionEntry& value) {
	std::stringstream str{};
	str << value;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

[[nodiscard]] bool operator==(const CompressionEntry& lhs, const CompressionEntry& rhs) {

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

		HttpHeaderFields http_header_fields = STBDS_ARRAY_EMPTY;

		CompressionSettingsCppPtr compression_settings =
		    get_compression_settings_cpp(http_header_fields);

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 0);
	}

	SUBCASE("standard simple list") {

		CompressionSettingsCppPtr compression_settings =
		    get_compression_setting_by_accept_encoding_header(" compress, gzip");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 2);

		CompressionEntry entry1 = compression_settings->entries[0];

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeCompress } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = compression_settings->entries[1];

		CompressionEntry entry2Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeGzip } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry2, entry2Expected);
	}

	SUBCASE("empty value") {

		CompressionSettingsCppPtr compression_settings =
		    get_compression_setting_by_accept_encoding_header("");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 0);
	}

	SUBCASE("'*' value") {

		CompressionSettingsCppPtr compression_settings =
		    get_compression_setting_by_accept_encoding_header(" *");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 1);

		CompressionEntry entry1 = compression_settings->entries[0];

		CompressionEntry entry1Expected = {
			.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 1.0F
		};

		REQUIRE_EQ(entry1, entry1Expected);
	}

	SUBCASE("complicated list with weights") {
		CompressionSettingsCppPtr compression_settings =
		    get_compression_setting_by_accept_encoding_header(" deflate;q=0.5, br;q=1.0");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 2);

		CompressionEntry entry1 = compression_settings->entries[0];

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeDeflate } },
			                                .weight = 0.5F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = compression_settings->entries[1];

		CompressionEntry entry2Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeBr } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry2, entry2Expected);
	}

	SUBCASE("complicated list with weights and 'identity'") {
		CompressionSettingsCppPtr compression_settings =
		    get_compression_setting_by_accept_encoding_header(
		        " zstd;q=1.0, identity; q=0.5, *;q=0");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 3);

		CompressionEntry entry1 = compression_settings->entries[0];

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueTypeNormalEncoding,
			                                           .data = { .normal_compression =
			                                                         CompressionTypeZstd } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = compression_settings->entries[1];

		CompressionEntry entry2Expected = {
			.value = { .type = CompressionValueTypeNoEncoding, .data = {} }, .weight = 0.5F
		};

		REQUIRE_EQ(entry2, entry2Expected);

		CompressionEntry entry3 = compression_settings->entries[2];

		CompressionEntry entry3Expected = {
			.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 0.0F
		};

		REQUIRE_EQ(entry3, entry3Expected);
	}
}

namespace {

struct ParsedURLWrapper {
  private:
	Http1Request* m_request;

  public:
	ParsedURLWrapper(Http1Request* request) : m_request{ request } {}

	~ParsedURLWrapper() {
		free_http1_request(m_request);
		m_request = nullptr;
	}

	[[nodiscard]] const ParsedURLPath& path() const { return m_request->head.request_line.path; }
};

std::unique_ptr<ParsedURLWrapper> parse_http1_request_path(const char* value) {
	std::string final_request = "GET ";
	final_request += value;
	final_request += " ";
	final_request += get_http_protocol_version_string(HTTPProtocolVersion1Dot1);
	final_request += "\r\n\r\n";

	HttpRequest* request = parse_http_request(strdup(final_request.c_str()), false);

	if(request == nullptr) {
		return nullptr;
	}

	if(request->type != HttpRequestTypeInternalV1) {
		return nullptr;
	}

	Http1Request* request1 = request->data.v1;

	return std::make_unique<ParsedURLWrapper>(request1);
}

} // namespace

TEST_CASE("testing the parsing of the http request") {

	SUBCASE("test url path parsing") {

		SUBCASE("simple url") {

			auto parsed_path = parse_http1_request_path("/");

			REQUIRE_NE(parsed_path, nullptr);

			const auto& path = parsed_path->path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/");

			REQUIRE_EQ(stbds_shlenu(path.search_path.hash_map), 0);
		}

		SUBCASE("real path url") {

			auto parsed_path = parse_http1_request_path("/test/hello");

			REQUIRE_NE(parsed_path, nullptr);

			const auto& path = parsed_path->path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/test/hello");

			REQUIRE_EQ(stbds_shlenu(path.search_path.hash_map), 0);
		}

		SUBCASE("path url with search parameters") {

			auto parsed_path = parse_http1_request_path("/test/hello?param1=hello&param2&param3=");

			REQUIRE_NE(parsed_path, nullptr);

			const auto& path = parsed_path->path();

			const auto path_comp = std::string{ path.path };

			REQUIRE_EQ(path_comp, "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			REQUIRE_EQ(stbds_shlenu(search_path.hash_map), 3);

			{

				ParsedSearchPathEntry* entry = find_search_key(search_path, "param1");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param1");

				REQUIRE_EQ(std::string{ entry->value }, "hello");
			}

			{

				ParsedSearchPathEntry* entry = find_search_key(search_path, "param2");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param2");

				REQUIRE_EQ(std::string{ entry->value }, "");
			}

			{
				ParsedSearchPathEntry* entry = find_search_key(search_path, "param3");

				REQUIRE_NE(entry, nullptr);

				REQUIRE_EQ(std::string{ entry->key }, "param3");

				REQUIRE_EQ(std::string{ entry->value }, "");
			}

			{

				ParsedSearchPathEntry* entry = find_search_key(search_path, "param4");

				REQUIRE_EQ(entry, nullptr);
			}
		}
	}
}
