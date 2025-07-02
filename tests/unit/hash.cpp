

#include <doctest.h>

#include <generic/hash.h>

#include "./c_types.hpp"

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

	constexpr ValueType& operator[](UnderlyingType::size_type n) noexcept { return m_value[n]; }

	[[nodiscard]] constexpr bool is_error() const { return m_is_error; }

	constexpr void set_error(bool error) { m_is_error = error; }

	[[nodiscard]] constexpr SizedBuffer get_sized_buffer() const {
		SizedBuffer sized_buffer = { .data = (void*)&this->m_value, .size = sha1_buffer_size };
		return sized_buffer;
	}

	friend std::ostream& operator<<(std::ostream& os, const Sha1BufferType& buffer);

	[[nodiscard]] bool operator==(const SizedBuffer& lhs) const {

		SizedBuffer rhs_sized_buffer = this->get_sized_buffer();

		return lhs == rhs_sized_buffer;
	}
};

std::ostream& operator<<(std::ostream& os, const Sha1BufferType& buffer) {
	SizedBuffer sized_buffer = buffer.get_sized_buffer();
	os << sized_buffer;
	return os;
}

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

[[nodiscard]] consteval Sha1BufferType operator""_sha1(const char* input, std::size_t size) {
	const auto result = get_expected_sha1_from_string(input, size);

	if(result.is_error()) {
		assert(false && "ERROR in consteval");
	}

	return result;
}

} // namespace

TEST_CASE("testing sha1 generation with openssl") {

	std::string sha1_provider = get_sha1_provider();
	REQUIRE_EQ(sha1_provider, "openssl (EVP)");

	SUBCASE("empty string") {

		constexpr const char* input_string = "";

		constexpr const auto expected_output = "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709"_sha1;

		const SizedBuffer result = get_sha1_from_string(input_string);

		REQUIRE_NE(result.data, nullptr);
		REQUIRE_NE(result.size, 0);

		REQUIRE_EQ(result, expected_output);
	}

	SUBCASE("simple string") {

		constexpr const char* input_string = "hello world";

		constexpr const auto expected_output = "2AAE6C35C94FCFB415DBE95F408B9CE91EE846ED"_sha1;

		const SizedBuffer result = get_sha1_from_string(input_string);

		REQUIRE_NE(result.data, nullptr);
		REQUIRE_NE(result.size, 0);

		REQUIRE_EQ(result, expected_output);
	}
}
