
#pragma once

#include "utils/sized_buffer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace helpers {

[[nodiscard]] SizedBuffer buffer_from_string(const std::string& inp);

[[nodiscard]] tstr tstr_from_utf8_string(const std::vector<std::uint8_t>& val);

[[nodiscard]] std::vector<std::uint8_t> vector_from_string(const std::string& data);

} // namespace helpers
