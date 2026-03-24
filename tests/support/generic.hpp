
#pragma once

#include "utils/sized_buffer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace helpers {

[[nodiscard]] ReadonlyBuffer buffer_from_string(const std::string& inp);

[[nodiscard]] tstr tstr_from_utf8_string(const std::vector<std::uint8_t>& val);

[[nodiscard]] std::vector<std::uint8_t> vector_from_string(const std::string& data);

[[nodiscard]] ReadonlyBuffer buffer_from_raw_data(const std::vector<std::uint8_t>& data);
} // namespace helpers
