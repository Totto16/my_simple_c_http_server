

#pragma once

#include <doctest.h>

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
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
		os << "SizedBuffer{content='0x";
		auto* buffer_ptr = static_cast<std::uint8_t*>(buffer.data);
		for(size_t i = 0; i < buffer.size; ++i) {
			os << get_hex_value_for_u8(buffer_ptr[i]);
		}
		os << "'}";
	}
	return os;
}

constexpr const size_t vector_max_for_printing_content = 40;

} // namespace

doctest::String toString(const SizedBuffer& buffer);

NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs);

namespace std {

template <typename T>
[[maybe_unused]] std::ostream& operator<<(std::ostream& os, const std::vector<T>& vector) {
	if(vector.data == NULL || vector.size > vector_max_for_printing_content) {
		os << "vector{data=" << vector.data() << ", size=" << vector.size() << "}";
	} else {
		os << "vector{content={";
		for(size_t i = 0; i < vector.size(); ++i) {
			if(i != 0) {
				os << ", ";
			}
			os << vector.at(i);
		}
		os << "} }";
	}
	return os;
}

doctest::String toString(const std::unordered_map<std::string, std::string>& string_map);

[[maybe_unused]] std::ostream&
operator<<(std::ostream& os, const std::unordered_map<std::string, std::string>& string_map);

} // namespace std
