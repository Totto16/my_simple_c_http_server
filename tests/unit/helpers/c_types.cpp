

#include "./c_types.hpp"

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

namespace {

constexpr const size_t buffer_max_for_printing_content = 40;

[[nodiscard]] std::string get_hex_value_for_u8(std::uint8_t value) {
	std::ostringstream oss;
	oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
	    << static_cast<int>(value);

	return oss.str();
}

} // namespace

std::ostream& operator<<(std::ostream& os, const SizedBuffer& buffer) {
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

doctest::String toString(const SizedBuffer& buffer) {
	std::stringstream str{};
	str << buffer;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs) {

	if(lhs.size != rhs.size) {
		return false;
	}

	if(lhs.data == NULL && rhs.data == NULL) {
		return true;
	}

	if(lhs.data == NULL || rhs.data == NULL) {
		return false;
	}

	auto* lhs_ptr = static_cast<std::uint8_t*>(lhs.data);
	auto* rhs_ptr = static_cast<std::uint8_t*>(rhs.data);

	for(size_t i = 0; i < lhs.size; ++i) {
		if(lhs_ptr[i] != rhs_ptr[i]) {
			return false;
		}
	}

	return true;
}
