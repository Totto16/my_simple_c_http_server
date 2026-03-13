#include "./http.hpp"

compression::CompressionSettingsCpp::CompressionSettingsCpp(HttpHeaderFields http_header_fields)
    : m_settings{ get_compression_settings(http_header_fields) } {}

compression::CompressionSettingsCpp::CompressionSettingsCpp(CompressionSettings settings)
    : m_settings{ settings } {}

compression::CompressionSettingsCpp::CompressionSettingsCpp(
    const char* const accept_encoding_value) {

	HttpHeaderFields http_header_fields = TVEC_EMPTY(HttpHeaderField);

	add_http_header_field(&http_header_fields, HTTP_HEADER_NAME(accept_encoding),
	                      tstr_from(accept_encoding_value));

	this->m_settings = get_compression_settings(http_header_fields);

	free_http_header_fields(&http_header_fields);
}

[[nodiscard]] const CompressionEntries& compression::CompressionSettingsCpp::entries() const {
	return this->m_settings.entries;
}

compression::CompressionSettingsCpp::~CompressionSettingsCpp() {
	free_compression_settings(this->m_settings);
}

[[nodiscard]] static const char* compression_type_to_string(CompressionType type) {
	const tstr temp = get_string_for_compress_format(type);
	return tstr_cstr(&temp);
}

[[nodiscard]] static const char* get_representation_for_compression_value(CompressionValue value) {
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

[[nodiscard]] bool operator==(const CompressionEntry& lhs, const CompressionEntry& rhs) {

	if(lhs.value != rhs.value) {
		return false;
	}

	return lhs.weight == rhs.weight;
}
