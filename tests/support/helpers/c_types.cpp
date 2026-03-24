

#include "./c_types.hpp"

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

[[nodiscard]] std::string get_hex_value_for_u8(std::uint8_t value) {
	std::ostringstream oss;
	oss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
	    << static_cast<int>(value);

	return oss.str();
}

constexpr const size_t buffer_max_for_printing_content = 40;

std::ostream& operator<<(std::ostream& os, const SizedBuffer& buffer) {
	if(buffer.data == NULL || buffer.size > buffer_max_for_printing_content) {
		os << "SizedBuffer{data=" << buffer.data << ", size=" << buffer.size << "}";
	} else {
		os << "SizedBuffer{content=";
		auto* buffer_ptr = static_cast<std::uint8_t*>(buffer.data);
		for(size_t i = 0; i < buffer.size; ++i) {
			if(i != 0) {
				os << ", ";
			}
			os << get_hex_value_for_u8(buffer_ptr[i]);
		}
		os << "'}";
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const ReadonlyBuffer& buffer) {
	if(buffer.data == NULL || buffer.size > buffer_max_for_printing_content) {
		os << "ReadonlyBuffer{data=" << buffer.data << ", size=" << buffer.size << "}";
	} else {
		os << "ReadonlyBuffer{content=";
		const auto* buffer_ptr = static_cast<const std::uint8_t*>(buffer.data);
		for(size_t i = 0; i < buffer.size; ++i) {
			if(i != 0) {
				os << ", ";
			}
			os << get_hex_value_for_u8(buffer_ptr[i]);
		}
		os << "'}";
	}
	return os;
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

	const auto* lhs_ptr = static_cast<const std::uint8_t*>(lhs.data);
	const auto* rhs_ptr = static_cast<const std::uint8_t*>(rhs.data);

	for(size_t i = 0; i < lhs.size; ++i) {
		if(lhs_ptr[i] != rhs_ptr[i]) {
			return false;
		}
	}

	return true;
}

NODISCARD static inline bool vec_is_eq_buffer(const SizedBuffer& lhs,
                                              const std::vector<std::uint8_t>& rhs) {

	if(lhs.size != rhs.size()) {
		return false;
	}

	if(lhs.data == NULL && rhs.data() == NULL) {
		return true;
	}

	if(lhs.data == NULL || rhs.data() == NULL) {
		return false;
	}

	const auto* lhs_ptr = static_cast<const std::uint8_t*>(lhs.data);
	const auto* rhs_ptr = static_cast<const std::uint8_t*>(rhs.data());

	for(size_t i = 0; i < lhs.size; ++i) {
		if(lhs_ptr[i] != rhs_ptr[i]) {
			return false;
		}
	}

	return true;
}

NODISCARD bool operator==(const SizedBuffer& lhs, const std::vector<std::uint8_t>& rhs) {
	return vec_is_eq_buffer(lhs, rhs);
}

NODISCARD bool operator==(const std::vector<std::uint8_t>& lhs, const SizedBuffer& rhs) {
	return vec_is_eq_buffer(rhs, lhs);
}

NODISCARD bool operator==(const ReadonlyBuffer& lhs, const ReadonlyBuffer& rhs) {
	if(lhs.size != rhs.size) {
		return false;
	}

	if(lhs.data == NULL && rhs.data == NULL) {
		return true;
	}

	if(lhs.data == NULL || rhs.data == NULL) {
		return false;
	}

	const auto* lhs_ptr = static_cast<const std::uint8_t*>(lhs.data);
	const auto* rhs_ptr = static_cast<const std::uint8_t*>(rhs.data);

	for(size_t i = 0; i < lhs.size; ++i) {
		if(lhs_ptr[i] != rhs_ptr[i]) {
			return false;
		}
	}

	return true;
}

NODISCARD bool operator==(const ReadonlyBuffer& lhs, const SizedBuffer& rhs) {
	return lhs == readonly_buffer_from_sized_buffer(rhs);
}

NODISCARD bool operator==(const SizedBuffer& lhs, const ReadonlyBuffer& rhs) {
	return readonly_buffer_from_sized_buffer(lhs) == rhs;
}
