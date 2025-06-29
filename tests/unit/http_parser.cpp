

#include <doctest.h>
#include <stb/ds.h>

#include <http/compression.h>
#include <http/http_protocol.h>

#include <ostream>
#include <sstream>
#include <string>

namespace {

NODISCARD CompressionSettings*
get_compression_setting_by_accept_encoding_header(const char* acceptEncodingValue) {
	HttpHeaderFields http_header_fields = STBDS_ARRAY_EMPTY;

	HttpHeaderField accept_encoding = { .key = strdup("Accept-Encoding"),
		                                .value = strdup(acceptEncodingValue) };

	stbds_arrput(http_header_fields, accept_encoding);

	CompressionSettings* compression_settings = getCompressionSettings(http_header_fields);

	return compression_settings;
}

[[nodiscard, maybe_unused]] const char* compression_type_to_string(COMPRESSION_TYPE type) {
	return get_string_for_compress_format(type);
}

[[nodiscard]] const char* get_representation_for_compression_value(CompressionValue value) {
	switch(value.type) {
		case CompressionValueType_NO_ENCODING: return "'identity'";
		case CompressionValueType_ALL_ENCODINGS: return "'*'";
		case CompressionValueType_NORMAL_ENCODING:
			return compression_type_to_string(value.data.normal_compression);
		default: assert("UNREACHABLE");
	}
}

[[nodiscard]] bool operator==(const CompressionValue& lhs, const CompressionValue& rhs) {

	if(lhs.type != rhs.type) {
		return false;
	}

	if(lhs.type == CompressionValueType_NORMAL_ENCODING) {
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

	SUBCASE("standard simple list") {

		CompressionSettings* compression_settings =
		    get_compression_setting_by_accept_encoding_header(" compress, gzip");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 2);

		CompressionEntry entry1 = compression_settings->entries[0];

		CompressionEntry entry1Expected = { .value = { .type = CompressionValueType_NORMAL_ENCODING,
			                                           .data = { .normal_compression =
			                                                         COMPRESSION_TYPE_COMPRESS } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry1, entry1Expected);

		CompressionEntry entry2 = compression_settings->entries[1];

		CompressionEntry entry2Expected = { .value = { .type = CompressionValueType_NORMAL_ENCODING,
			                                           .data = { .normal_compression =
			                                                         COMPRESSION_TYPE_GZIP } },
			                                .weight = 1.0F };

		REQUIRE_EQ(entry2, entry2Expected);
	}

	SUBCASE("empty value") {

		CompressionSettings* compression_settings =
		    get_compression_setting_by_accept_encoding_header("");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 0);
	}

	SUBCASE("'*' value") {

		CompressionSettings* compression_settings =
		    get_compression_setting_by_accept_encoding_header(" *");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 1);

		// TODO(Totto):finish checks
	}

	SUBCASE("complicated list with weights") {
		CompressionSettings* compression_settings =
		    get_compression_setting_by_accept_encoding_header(" compress;q=0.5, gzip;q=1.0");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 2);

		// TODO(Totto):finish checks
	}

	SUBCASE("complicated list with weights and 'identity'") {
		CompressionSettings* compression_settings =
		    get_compression_setting_by_accept_encoding_header(
		        " gzip;q=1.0, identity; q=0.5, *;q=0");

		REQUIRE_NE(compression_settings, nullptr);

		size_t entries_length = stbds_arrlenu(compression_settings->entries);

		REQUIRE_EQ(entries_length, 3);

		// TODO(Totto):finish checks
	}
}
