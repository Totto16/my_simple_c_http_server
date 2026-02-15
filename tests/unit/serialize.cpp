#include <generic/serialize.h>

#include "helpers/c_types.hpp"

#include <array>
#include <functional>
#include <vector>

struct TestCaseDeserialize32Value {
	std::array<uint8_t, 4> input;
	uint32_t result;
};

struct TestCaseDeserialize32 {
	doctest::String name;
	std::vector<TestCaseDeserialize32Value> values;
	std::function<uint32_t(const uint8_t*)> fn;
};

[[nodiscard]] static uint32_t select_native_value_u32(uint32_t LE_value, uint32_t BE_value) {

	if constexpr(std::endian::native == std::endian::little) {
		return LE_value;
	} else if constexpr(std::endian::native == std::endian::big) {
		return BE_value;
	} else {
		assert(false && "unreachable");
	}
}

[[nodiscard]] static uint16_t select_native_value_u16(uint16_t LE_value, uint16_t BE_value) {

	if constexpr(std::endian::native == std::endian::little) {
		return LE_value;
	} else if constexpr(std::endian::native == std::endian::big) {
		return BE_value;
	} else {
		assert(false && "unreachable");
	}
}

TEST_CASE("testing deserializing u32") {

	std::vector<TestCaseDeserialize32> test_cases = {
		{
		    .name = "le to no",
		    .values = { {
				.input = { 1, 2, 3, 4 },
		    	.result = 0x01020304ULL,
		},
        {.input = {0, 0, 0, 1},.result =  0x00000001,},
        {.input = {0xFF, 0, 0, 0}, .result = 0xFF000000},
        {.input = {0x12,0x34,0x56,  0x78, },.result =  0x12345678},
		{.input = {0x78,0x56,0x34, 0x12,  },.result =  0x78563412},
		},
		
		    .fn = deserialize_u32_le_to_no,
		},
//
	{
		    .name = "le to host",
		    .values = { {
				.input = { 1, 2, 3, 4 },
		    	.result = select_native_value_u32(0x04030201,0x01020304ULL),
		},
        {.input = {0, 0, 0, 1},.result =  select_native_value_u32( 0x01000000,0x00000001),},
        {.input = {0xFF, 0, 0, 0}, .result = select_native_value_u32(0x000000FF,0xFF000000)},
        {.input = {0x12,0x34,0x56,  0x78, },.result =  select_native_value_u32(0x78563412,0x12345678)},
		{.input = {0x78,0x56,0x34, 0x12,  },.result =  select_native_value_u32(0x12345678,0x78563412)},
		},
		    .fn = deserialize_u32_le_to_host,
		},
//
	{
		    .name = "be to host",
		    .values = { {
				.input = { 1, 2, 3, 4 },
		    	.result = select_native_value_u32(0x01020304ULL,0x04030201),
		},
        {.input = {0, 0, 0, 1},.result =  select_native_value_u32( 0x00000001,0x01000000),},
        {.input = {0xFF, 0, 0, 0}, .result = select_native_value_u32(0xFF000000,0x000000FF)},
        {.input = {0x12,0x34,0x56,  0x78, },.result =  select_native_value_u32(0x12345678,0x78563412)},
		{.input = {0x78,0x56,0x34, 0x12,  },.result =  select_native_value_u32(0x78563412,0x12345678)},
		},
		    .fn = deserialize_u32_be_to_host,
		},
	};

	for(const auto& test_case : test_cases) {

		SUBCASE(test_case.name) {

			for(size_t i = 0; i < test_case.values.size(); ++i) {
				const auto value = test_case.values.at(i);

				const auto case_str = std::string{ "Case " } + std::to_string(i);
				doctest::String case_name = doctest::String{ case_str.c_str() };

				SUBCASE(case_name) {

					const uint32_t result = test_case.fn(value.input.data());

					REQUIRE_EQ(result, value.result);
				}
			}
		}
	}
}

struct TestCaseDeserialize16Value {
	std::array<uint8_t, 2> input;
	uint16_t result;
};

struct TestCaseDeserialize16 {
	doctest::String name;
	std::vector<TestCaseDeserialize16Value> values;
	std::function<uint16_t(const uint8_t*)> fn;
};

TEST_CASE("testing deserializing u16") {

	std::vector<TestCaseDeserialize16> test_cases = {
		{
		    .name = "le to no",
		    .values = { {
				.input = { 1, 2},
		    	.result = 0x0102,
		},
        {.input = {0, 1},.result =  0x0001,},
        {.input = {0xFF, 0}, .result = 0xFF00},
        {.input = {0x12,0x34 },.result =  0x1234},
		{.input = {0x78,0x56, },.result =  0x7856},
		},
		
		    .fn = deserialize_u16_le_to_no,
		},
//
	{
		    .name = "le to host",
		    .values = { {
				.input = { 1, 2 },
		    	.result = select_native_value_u16(0x0201,0x0102),
		},
        {.input = {0, 1},.result =  select_native_value_u16( 0x0100,0x0001),},
        {.input = {0xFF, 0, }, .result = select_native_value_u16(0x00FF,0xFF00)},
        {.input = {0x12,0x34 },.result =  select_native_value_u16(0x3412,0x1234)},
		{.input = {0x78,0x56,  },.result =  select_native_value_u16(0x5678,0x7856)},
		},
		    .fn = deserialize_u16_le_to_host,
		},
//
	{
		    .name = "be to host",
		    .values = { {
				.input = { 1, 2, },
		    	.result = select_native_value_u16(0x0102,0x0201),
		},
        {.input = {0,  1},.result =  select_native_value_u16( 0x0001,0x0100),},
        {.input = {0xFF, 0, }, .result = select_native_value_u16(0xFF00,0x00FF)},
        {.input = {0x12,0x34 },.result =  select_native_value_u16(0x1234,0x3412)},
		{.input = {0x78,0x56  },.result =  select_native_value_u16(0x7856,0x5678)},
		},
		    .fn = deserialize_u16_be_to_host,
		},
	};

	for(const auto& test_case : test_cases) {

		SUBCASE(test_case.name) {

			for(size_t i = 0; i < test_case.values.size(); ++i) {
				const auto value = test_case.values.at(i);

				const auto case_str = std::string{ "Case " } + std::to_string(i);
				doctest::String case_name = doctest::String{ case_str.c_str() };

				SUBCASE(case_name) {

					const uint16_t result = test_case.fn(value.input.data());

					REQUIRE_EQ(result, value.result);
				}
			}
		}
	}
}
