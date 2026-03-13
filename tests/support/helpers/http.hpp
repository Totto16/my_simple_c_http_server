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
