#pragma once
#include <utils/sized_buffer.h>

#include <cstdint>
#include <string>

[[nodiscard]] std::string get_hex_value_for_u8(std::uint8_t value);

NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs);

std::ostream& operator<<(std::ostream& os, const SizedBuffer& buffer);
