#pragma once

#include <http/compression.h>
#include <http/header.h>
#include <http/parser.h>
#include <http/protocol.h>

#include <ostream>

namespace compression {
class CompressionSettingsCpp {
  private:
	CompressionSettings m_settings;

  public:
	CompressionSettingsCpp(HttpHeaderFields http_header_fields);

	CompressionSettingsCpp(CompressionSettings settings);

	CompressionSettingsCpp(const char* const accept_encoding_value);

	[[nodiscard]] const CompressionEntries& entries() const;

	CompressionSettingsCpp(CompressionSettingsCpp&&) = delete;

	CompressionSettingsCpp(const CompressionSettingsCpp&) = delete;

	CompressionSettingsCpp& operator=(const CompressionSettingsCpp&) = delete;

	CompressionSettingsCpp operator=(CompressionSettingsCpp&&) = delete;

	~CompressionSettingsCpp();
};

} // namespace compression

[[nodiscard]] bool operator==(const CompressionValue& lhs, const CompressionValue& rhs);

std::ostream& operator<<(std::ostream& os, const CompressionEntry& entry);

[[nodiscard]] bool operator==(const CompressionEntry& lhs, const CompressionEntry& rhs);

namespace http {

struct ParsedURIWrapper {
  private:
	ParsedRequestUriResult m_result;

  public:
	ParsedURIWrapper(ParsedRequestUriResult result);

	static ParsedURIWrapper parse(const std::string& uri);

	[[nodiscard]] const ParsedURLPath& path() const;

	[[nodiscard]] const char* error() const;

	~ParsedURIWrapper();
};

} // namespace http

namespace serialize {

[[nodiscard]] uint32_t select_native_value_u32(uint32_t LE_value, uint32_t BE_value);

[[nodiscard]] uint16_t select_native_value_u16(uint16_t LE_value, uint16_t BE_value);
} // namespace serialize
