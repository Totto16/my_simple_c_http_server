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

http::ParsedURIWrapper::ParsedURIWrapper(ParsedRequestURIResult result) : m_result{ result } {}

[[nodiscard]] const ParsedURLPath& http::ParsedURIWrapper::path() const {
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

[[nodiscard]] const char* http::ParsedURIWrapper::error() const {
	if(m_result.is_error) {
		return m_result.value.error;
	}

	return NULL;
}

http::ParsedURIWrapper::~ParsedURIWrapper() {
	if(m_result.is_error) {
		return;
	}

	free_parsed_request_uri(this->m_result.value.uri);
}

http::ParsedURIWrapper http::ParsedURIWrapper::parse(const std::string& uri) {

	auto result = parse_request_uri(tstr_view{ uri.data(), uri.size() });

	return ParsedURIWrapper{ result };
}

[[nodiscard]] uint32_t serialize::select_native_value_u32(uint32_t LE_value, uint32_t BE_value) {

	if constexpr(std::endian::native == std::endian::little) {
		return LE_value;
	} else if constexpr(std::endian::native == std::endian::big) {
		return BE_value;
	} else {
		assert(false && "unreachable");
	}
}

[[nodiscard]] uint16_t serialize::select_native_value_u16(uint16_t LE_value, uint16_t BE_value) {

	if constexpr(std::endian::native == std::endian::little) {
		return LE_value;
	} else if constexpr(std::endian::native == std::endian::big) {
		return BE_value;
	} else {
		assert(false && "unreachable");
	}
}
