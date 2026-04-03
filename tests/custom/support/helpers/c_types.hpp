#pragma once
#include <utils/sized_buffer.h>

#include <cstdint>
#include <string>
#include <vector>

[[nodiscard]] std::string get_hex_value_for_u8(std::uint8_t value);

NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs);

NODISCARD bool operator==(const SizedBuffer& lhs, const std::vector<std::uint8_t>& rhs);

NODISCARD bool operator==(const std::vector<std::uint8_t>& lhs, const SizedBuffer& rhs);

std::ostream& operator<<(std::ostream& os, const SizedBuffer& buffer);

std::ostream& operator<<(std::ostream& os, const ReadonlyBuffer& buffer);

NODISCARD bool operator==(const ReadonlyBuffer& lhs, const ReadonlyBuffer& rhs);

NODISCARD bool operator==(const ReadonlyBuffer& lhs, const SizedBuffer& rhs);

NODISCARD bool operator==(const SizedBuffer& lhs, const ReadonlyBuffer& rhs);

#define REQUIRE_TRUE(val) REQUIRE(val)
