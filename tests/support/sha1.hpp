
#pragma once

#include <utils/utils.h>

#include <array>
#include <cstdint>

#include <ostream>
#include <utils/sized_buffer.h>

namespace details {

[[nodiscard]] consteval std::uint8_t single_hex_number(char input, OUT_PARAM(bool) success) {
	if(input >= '0' && input <= '9') {
		*success = true;
		return static_cast<std::uint8_t>(input - '0');
	}

	if(input >= 'A' && input <= 'F') {
		*success = true;
		return static_cast<std::uint8_t>(input - 'A' + 10);
	}

	if(input >= 'a' && input <= 'f') {
		*success = true;
		return static_cast<std::uint8_t>(input - 'a' + 10);
	}

	*success = false;
	return 0;
}

[[nodiscard]] consteval std::uint8_t single_hex_value(const char* input, OUT_PARAM(bool) success) {

	const auto first = single_hex_number(input[0], success);

	if(!(*success)) {
		return 0;
	}

	const auto second = single_hex_number(input[1], success);

	if(!(*success)) {
		return 0;
	}

	*success = true;
	return (first << 4) | second;
}

constexpr const size_t sha1_buffer_size = 20;

struct Sha1BufferType {
  public:
	using ValueType = std::uint8_t;
	using UnderlyingType = std::array<ValueType, sha1_buffer_size>;

  private:
	UnderlyingType m_value;
	bool m_is_error;

  public:
	constexpr Sha1BufferType(UnderlyingType&& value)
	    : m_value{ std::move(value) }, m_is_error{ false } {}

	constexpr Sha1BufferType(bool is_error) : m_value{}, m_is_error{ is_error } {}

	constexpr ValueType& operator[](UnderlyingType::size_type n) noexcept {
		if(is_error()) {
			assert(false && "can't index a value with an error");
			exit(1);
		}

		return m_value[n];
	}

	[[nodiscard]] constexpr bool is_error() const { return m_is_error; }

	constexpr void set_error(bool error) { m_is_error = error; }

	[[nodiscard]] SizedBuffer get_sized_buffer() const;

	friend std::ostream& operator<<(std::ostream& os, const Sha1BufferType& buffer);

	[[nodiscard]] bool operator==(const SizedBuffer& lhs) const;
};

[[nodiscard]] consteval Sha1BufferType get_expected_sha1_from_string(const char* input,
                                                                     std::size_t size) {

	Sha1BufferType result = { false };

	if(size == 0) {
		return result;
	}

	if(size % 2 != 0) {
		return result;
	}

	size_t buffer_size = size / 2;

	if(buffer_size != sha1_buffer_size) {
		return result;
	}

	for(size_t i = 0; i < buffer_size; ++i) {
		bool success_sub = true;
		std::uint8_t value = single_hex_value(input + (i * 2), &success_sub);
		if(!success_sub) {
			return result;
		}
		result[i] = value;
	}

	result.set_error(false);
	return result;
}

} // namespace details

std::ostream& operator<<(std::ostream& os, const details::Sha1BufferType& buffer);

namespace sha1 {
using Sha1BufferType = details::Sha1BufferType;

} // namespace sha1

[[nodiscard]] consteval sha1::Sha1BufferType operator""_sha1(const char* input, std::size_t size) {
	const auto result = details::get_expected_sha1_from_string(input, size);

	if(result.is_error()) {
		assert(false && "ERROR in consteval");
	}

	return result;
}
