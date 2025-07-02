

#include <doctest.h>

#include <generic/hash.h>

#include "./c_types.hpp"

#include <expected>

namespace {

[[nodiscard]] consteval std::uint8_t single_hex_number(char input, bool* success) {
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
	;
}

[[nodiscard]] consteval std::uint8_t single_hex_value(const char* input, bool* success) {

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

using Sha1BufferType = std::array<std::uint8_t, sha1_buffer_size>;

[[nodiscard]] consteval Sha1BufferType
get_expected_sha1_from_string(const char* input, std::size_t size, bool* success) {

	Sha1BufferType result = {};

	if(size == 0) {
		*success = false;
		return result;
	}

	if(size % 2 != 0) {
		*success = false;
		return result;
	}

	size_t buffer_size = size / 2;

	if(buffer_size != sha1_buffer_size) {
		*success = false;
		return result;
	}

	for(size_t i = 0; i < buffer_size; ++i) {
		bool success_sub = true;
		std::uint8_t value = single_hex_value(input + (i * 2), &success_sub);
		if(!success_sub) {
			*success = false;
			return result;
		}
		result[i] = value;
	}

	*success = true;
	return result;
}

[[nodiscard]] consteval Sha1BufferType operator""_sha1(const char* input, std::size_t size) {
	bool success = true;
	const auto result = get_expected_sha1_from_string(input, size, &success);

	if(!success) {
		assert(false && "ERROR in consteval");
	}

	return result;
}

[[nodiscard]] SizedBuffer
get_sized_buffer_from_sha1_return_type(const Sha1BufferType& return_type) {
	SizedBuffer sized_buffer = { .data = (void*)&return_type, .size = sha1_buffer_size };
	return sized_buffer;
}

std::ostream& operator<<(std::ostream& os, const Sha1BufferType& sha1_buffer) {
	SizedBuffer buffer = get_sized_buffer_from_sha1_return_type(sha1_buffer);
	os << buffer;
	return os;
}

} // namespace

doctest::String toString(const Sha1BufferType& buffer) {
	std::stringstream str{};
	str << buffer;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

NODISCARD bool operator==(const SizedBuffer& lhs, const Sha1BufferType& rhs) {

	SizedBuffer rhs_sized_buffer = get_sized_buffer_from_sha1_return_type(rhs);

	return lhs == rhs_sized_buffer;
}

TEST_CASE("testing sha1 generation with openssl") {

	std::string sha1_provider = get_sha1_provider();
	REQUIRE_EQ(sha1_provider, "openssl (EVP)");

	SUBCASE("simple string") {

		constexpr const char* input_string = "hello world";

		constexpr const auto expected_output = "22596363B3DE40B06F981FB85D82312E8C0ED511"_sha1;

		const SizedBuffer result = get_sha1_from_string(input_string);

		REQUIRE_NE(result.data, nullptr);
		REQUIRE_NE(result.size, 0);

		REQUIRE_EQ(result, expected_output);
	}
}
