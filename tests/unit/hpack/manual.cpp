#include "../c_types.hpp"

#include "./helpers.hpp"

TEST_CASE("testing hpack deserializing - manual tests") {

	constexpr size_t DEFAULT_HEADER_TABLE_SIZE = 4096;

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	const std::vector<ThirdPartyHpackTestCaseEntry> test_cases = {

	};

	HpackStateCpp state = get_default_hpack_state_cpp(DEFAULT_HEADER_TABLE_SIZE);

	REQUIRE_NE(state.get(), nullptr);

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str2 = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str2.c_str() };

		INFO("the sequential number of that hpack packet has to be the same as the "
		     "index: ",
		     test_case.seqno, " | ", i);
		REQUIRE_EQ(test_case.seqno, i);

		SUBCASE(case_name) {

			const auto input = buffer_from_raw_data(test_case.wire_data);

			const auto result = http2_hpack_decompress_data(state.get(), input);

			std::string error = "";
			if(result.is_error) {
				error = std::string{ result.data.error };

				INFO("Error occurred: ", error);
				CHECK_FALSE(result.is_error);
				continue;
			}

			REQUIRE_FALSE(result.is_error);

			const auto actual_result = result.data.result;

			const auto& expected_result = test_case.headers;

			const auto actual_result_cpp = get_cpp_headers(actual_result);

			REQUIRE_EQ(actual_result_cpp, expected_result);
		}
	}
}

struct IntegerTest {
	std::vector<std::uint8_t> values;
	std::uint8_t prefix_bits;
	HpackVariableInteger result;
};

TEST_CASE("testing hpack deserializing - integer tests") {

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.1
	const std::vector<IntegerTest> test_cases = {
		{ .values = { 0b01010 }, .prefix_bits = 5, .result = 10 },
		{ .values = { 0b11111, 0b10011010, 0b00001010 }, .prefix_bits = 5, .result = 1337 },
		{ .values = { 0b00101010 }, .prefix_bits = 8, .result = 42 },
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str2 = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str2.c_str() };

		SUBCASE(case_name) {

			const auto input = buffer_from_raw_data(test_case.values);

			size_t pos = 0;

			const auto result = decode_hpack_variable_integer(
			    &pos, input.size, (std::uint8_t*)input.data, test_case.prefix_bits);

			std::string error = "";
			if(result.is_error) {
				error = std::string{ result.data.error };

				INFO("Error occurred: ", error);
			}

			REQUIRE_FALSE(result.is_error);

			const auto actual_result = result.data.value;

			const auto& expected_result = test_case.result;

			REQUIRE_EQ(actual_result, expected_result);

			REQUIRE_EQ(pos, input.size);
		}
	}

	// test possible errors

	{ // case one, too much data
		const std::vector<std::uint8_t> raw_data = { 0b01010, 0x03F };
		const size_t prefix_bits = 5;

		//
		const auto input = buffer_from_raw_data(raw_data);

		size_t pos = 0;

		const auto result =
		    decode_hpack_variable_integer(&pos, input.size, (std::uint8_t*)input.data, prefix_bits);

		REQUIRE_FALSE(result.is_error);

		REQUIRE_NE(pos, input.size);
	}

	{ // case two, not enough data
		const std::vector<std::uint8_t> raw_data = { 0b11111, 0b10011010 };
		const size_t prefix_bits = 5;

		//
		const auto input = buffer_from_raw_data(raw_data);

		size_t pos = 0;

		const auto result =
		    decode_hpack_variable_integer(&pos, input.size, (std::uint8_t*)input.data, prefix_bits);

		REQUIRE(result.is_error);

		const std::string expected_error = "not enough bytes";
		const std::string actual_error = result.data.error;

		REQUIRE_EQ(expected_error, actual_error);
	}

	{ // case three, too much data, uint64_t  would overflow
		const std::vector<std::uint8_t> raw_data = { 0b11111,    0b10011010, 0b10011010, 0b10011010,
			                                         0b10011010, 0b10011010, 0b10011010, 0b10011010,
			                                         0b10011010, 0b10011010, 0b10011010, 0b10011010,
			                                         0b10011010, 0b10011010, 0b10011010 };
		const size_t prefix_bits = 5;

		//
		const auto input = buffer_from_raw_data(raw_data);

		size_t pos = 0;

		const auto result =
		    decode_hpack_variable_integer(&pos, input.size, (std::uint8_t*)input.data, prefix_bits);

		REQUIRE(result.is_error);

		const std::string expected_error = "final integer would be too big";
		const std::string actual_error = result.data.error;

		REQUIRE_EQ(expected_error, actual_error);
	}
}

namespace test {

struct DynamicTable {
	std::vector<std::pair<std::string, std::string>> entries;
	size_t size;
};

[[maybe_unused]] static doctest::String toString(const DynamicTable& table) {
	std::stringstream str{};
	str << "DynamicTable:\n" << table.entries;
	str << "\n" << table.size << "\n";

	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

NODISCARD [[maybe_unused]] static bool operator==(const DynamicTable& table1,
                                                  const DynamicTable& table2) {

	if(table1.size != table2.size) {
		return false;
	}

	const auto table1_vec = table1.entries;
	const auto table2_vec = table2.entries;

	if(table1_vec.size() != table2_vec.size()) {
		return false;
	}

	for(size_t i = 0; i < table1_vec.size(); ++i) {
		if(table1_vec.at(i) != table2_vec.at(i)) {
			return false;
		}
	}

	return true;
}

} // namespace test

namespace {
[[maybe_unused]] std::ostream& operator<<(std::ostream& os, const test::DynamicTable& table) {
	os << toString(table);
	return os;
}
} // namespace



struct HeaderFieldTest {
	std::vector<std::uint8_t> raw_data;
	test::DynamicTable dynamic_table;
	std::unordered_map<std::string, std::string> result;
};

// this is a hack, as I don't want to expose the state in the header for c, but i also want to test
// it, i am willing to copy paste this for the tests
namespace cpp_forbidden_test_type_impl_DONT_USE {

extern "C" {

typedef struct {
	char* key;
	char* value;
} HpackHeaderDynamicEntry;

TVEC_DEFINE_VEC_TYPE(HpackHeaderDynamicEntry)

typedef TVEC_TYPENAME(HpackHeaderDynamicEntry) HpackHeaderDynamicEntries;

struct HpackStateImpl {
	HpackHeaderDynamicEntries dynamic_table;
	size_t max_dynamic_table_byte_size;
	size_t current_dynamic_table_byte_size;
};
}

} // namespace cpp_forbidden_test_type_impl_DONT_USE

[[nodiscard]] test::DynamicTable get_dynamic_table(const auto& state) {

	const auto* state_cpp_extracted =
	    (cpp_forbidden_test_type_impl_DONT_USE::HpackStateImpl*)state.get();

	const size_t size = state_cpp_extracted->current_dynamic_table_byte_size;

	std::vector<std::pair<std::string, std::string>> entries = {};

	for(size_t i = 0; i < TVEC_LENGTH(HpackHeaderDynamicEntry, state_cpp_extracted->dynamic_table);
	    ++i) {
		cpp_forbidden_test_type_impl_DONT_USE::HpackHeaderDynamicEntry entry =
		    cpp_forbidden_test_type_impl_DONT_USE::TVEC_AT(HpackHeaderDynamicEntry,
		                                                   state_cpp_extracted->dynamic_table, i);

		entries.emplace_back(entry.key, entry.value);
	}

	return test::DynamicTable{ .entries = entries, .size = size };
}

TEST_CASE("testing hpack deserializing - header field tests") {

	constexpr size_t DEFAULT_HEADER_TABLE_SIZE = 4096;

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.2
	const std::vector<HeaderFieldTest> test_cases = {
		{ 
			.raw_data =parse_wire_data("400a637573746f6d2d6b65790d637573746f6d2d686561646572"),
			.dynamic_table = {
				.entries = { {"custom-key" , "custom-header"}},
				.size  =  55,
			},
			.result = {
				{"custom-key" , "custom-header"}
			}
		},
		{ 
			.raw_data =parse_wire_data("040c2f73616d706c652f70617468"),
			.dynamic_table = {
				.entries = { },
				.size  =  0,
			},
			.result = {
				{":path","/sample/path"}
			}
		},
		{ 
			.raw_data =parse_wire_data("100870617373776f726406736563726574"),
			.dynamic_table = {
				.entries = { },
				.size  =  0,
			},
			.result = {
				{"password","secret"}
			}
		},
		{ 
			.raw_data =parse_wire_data("82"),
			.dynamic_table = {
				.entries = { },
				.size  =  0,
			},
			.result = {
				{":method","GET1"}
			}
		}
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		HpackStateCpp state = get_default_hpack_state_cpp(DEFAULT_HEADER_TABLE_SIZE);
		REQUIRE_NE(state.get(), nullptr);

		const auto& test_case = test_cases.at(i);

		const auto case_str2 = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str2.c_str() };

		SUBCASE(case_name) {

			const auto input = buffer_from_raw_data(test_case.raw_data);

			const auto result = http2_hpack_decompress_data(state.get(), input);

			std::string error = "";
			if(result.is_error) {
				error = std::string{ result.data.error };

				INFO("Error occurred: ", error);
				CHECK_FALSE(result.is_error);
				continue;
			}

			REQUIRE_FALSE(result.is_error);

			const auto actual_result = result.data.result;

			const auto& expected_result = test_case.result;

			const auto actual_result_cpp = get_cpp_headers(actual_result);

			REQUIRE_EQ(actual_result_cpp, expected_result);

			const auto& expected_dynamic_table = test_case.dynamic_table;

			const auto actual_dynamic_table = get_dynamic_table(state);

			REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
		}
	}
}
