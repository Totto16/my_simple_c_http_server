

#pragma once

#include <doctest.h>

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include <utils/sized_buffer.h>

namespace {

constexpr const size_t buffer_max_for_printing_content = 40;

[[nodiscard]] std::string get_hex_value_for_u8(std::uint8_t value) {
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
	    << static_cast<int>(value);

	return oss.str();
}

[[maybe_unused]] std::ostream& operator<<(std::ostream& os, const SizedBuffer& buffer) {
	if(buffer.data == NULL || buffer.size > buffer_max_for_printing_content) {
		os << "SizedBuffer{data=" << buffer.data << ", size=" << buffer.size << "}";
	} else {
		os << "SizedBuffer{content='";
		auto* buffer_ptr = static_cast<std::uint8_t*>(buffer.data);
		for(size_t i = 0; i < buffer.size; ++i) {
			os << get_hex_value_for_u8(buffer_ptr[i]);
		}
		os << "'}";
	}
	return os;
}

} // namespace

doctest::String toString(const SizedBuffer& buffer);

NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs);

NODISCARD bool operator==(const SizedBuffer& lhs, const std::vector<std::uint8_t>& rhs);
