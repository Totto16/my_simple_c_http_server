#include "helpers/c_types.hpp"
#include "helpers/cpp_types.hpp"

#include "generated_hpack_tests.hpp"

#include "./helpers.hpp"

TEST_SUITE_BEGIN("hpack/manual" * doctest::description("manual hpack tests") *
                 doctest::timeout(2.0));

struct IntegerTest {
	std::vector<std::uint8_t> values;
	std::uint8_t prefix_bits;
	HpackVariableInteger result;
};

TEST_CASE("testing hpack deserializing - integer tests <hpack_integer_deserialize>") {

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.1
	const std::vector<IntegerTest> test_cases = {
		{ .values = { 0b01010 }, .prefix_bits = 5, .result = 10 },
		{ .values = { 0b11111, 0b10011010, 0b00001010 }, .prefix_bits = 5, .result = 1337 },
		{ .values = { 0b00101010 }, .prefix_bits = 8, .result = 42 },
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
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
			}();
		}
	}

	// test possible errors

	SUBCASE("too much data") { // case one, too much data
		[]() -> void {
			const std::vector<std::uint8_t> raw_data = { 0b01010, 0x03F };
			const size_t prefix_bits = 5;

			//
			const auto input = buffer_from_raw_data(raw_data);

			size_t pos = 0;

			const auto result = decode_hpack_variable_integer(
			    &pos, input.size, (std::uint8_t*)input.data, prefix_bits);

			REQUIRE_FALSE(result.is_error);

			REQUIRE_NE(pos, input.size);
		}();
	}

	SUBCASE("not enough data") { // case two, not enough data
		[]() -> void {
			const std::vector<std::uint8_t> raw_data = { 0b11111, 0b10011010 };
			const size_t prefix_bits = 5;

			//
			const auto input = buffer_from_raw_data(raw_data);

			size_t pos = 0;

			const auto result = decode_hpack_variable_integer(
			    &pos, input.size, (std::uint8_t*)input.data, prefix_bits);

			REQUIRE(result.is_error);

			const std::string expected_error = "not enough bytes";
			const std::string actual_error = result.data.error;

			REQUIRE_EQ(expected_error, actual_error);
		}();
	}

	SUBCASE("number would overflow") { // case three, too much data, uint64_t  would overflow
		[]() -> void {
			const std::vector<std::uint8_t> raw_data = { 0b11111,    0b10011010, 0b10011010,
				                                         0b10011010, 0b10011010, 0b10011010,
				                                         0b10011010, 0b10011010, 0b10011010,
				                                         0b10011010, 0b10011010, 0b10011010,
				                                         0b10011010, 0b10011010, 0b10011010 };
			const size_t prefix_bits = 5;

			//
			const auto input = buffer_from_raw_data(raw_data);

			size_t pos = 0;

			const auto result = decode_hpack_variable_integer(
			    &pos, input.size, (std::uint8_t*)input.data, prefix_bits);

			REQUIRE(result.is_error);

			const std::string expected_error = "final integer would be too big";
			const std::string actual_error = result.data.error;

			REQUIRE_EQ(expected_error, actual_error);
		}();
	}
}

namespace test {

struct DynamicTable {
	std::vector<std::pair<std::string, std::string>> entries;
	size_t size;
};

[[maybe_unused]] static std::ostream& operator<<(std::ostream& os,
                                                 const test::DynamicTable& table) {
	os << "DynamicTable:\n" << os_stream_formattable_to_doctest(table.entries);
	os << "\n" << table.size << "\n";
	return os;
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

struct HeaderFieldDeserializeTest {
	std::vector<std::uint8_t> raw_data;
	test::DynamicTable dynamic_table;
	std::vector<std::pair<std::string, std::string>> result;
};

// this is a hack, as I don't want to expose the state in the header for c, but i also want to test
// it, i am willing to copy paste this for the tests
namespace cpp_forbidden_test_type_impl_DONT_USE {

extern "C" {

#include "http/dynamic_hpack_table.h"

typedef struct {
	HpackHeaderDynamicTable dynamic_table;
	size_t max_dynamic_table_byte_size;
	size_t current_dynamic_table_byte_size;
} HpackDynamicTableState;

struct HpackDecompressStateImpl {
	HpackDynamicTableState dynamic_table_state;
};

struct HpackCompressStateImpl {
	HpackDynamicTableState dynamic_table_state;
};
}

} // namespace cpp_forbidden_test_type_impl_DONT_USE

[[nodiscard]] static test::DynamicTable get_dynamic_table(
    const cpp_forbidden_test_type_impl_DONT_USE::HpackDynamicTableState* const state) {

	const size_t size = state->current_dynamic_table_byte_size;

	std::vector<std::pair<std::string, std::string>> entries = {};

	for(size_t i = state->dynamic_table.start, rest_count = state->dynamic_table.count;
	    rest_count != 0; i = (i + 1) % state->dynamic_table.capacity, rest_count--) {
		const auto& entry = state->dynamic_table.entries[i];

		entries.emplace_back(string_from_tstr(entry.key), string_from_tstr(entry.value));
	}

	return test::DynamicTable{ .entries = entries, .size = size };
}

[[nodiscard]] static test::DynamicTable
get_dynamic_decompress_table(const HpackDecompressStateCpp& state) {

	const auto* state_cpp_extracted =
	    (const cpp_forbidden_test_type_impl_DONT_USE::HpackDecompressStateImpl*)state.get();

	return get_dynamic_table(&(state_cpp_extracted->dynamic_table_state));
}

[[nodiscard]] static test::DynamicTable
get_dynamic_compress_table(const HpackCompressStateCpp& state) {

	const auto* state_cpp_extracted =
	    (const cpp_forbidden_test_type_impl_DONT_USE::HpackCompressStateImpl*)state.get();

	return get_dynamic_table(&(state_cpp_extracted->dynamic_table_state));
}

TEST_CASE("testing hpack deserializing - header field tests <hpack_header_fields>") {

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.2
	const std::vector<HeaderFieldDeserializeTest> test_cases = {
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
				{":method","GET"}
			}
		}
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
				HpackDecompressStateCpp decompress_state =
				    get_default_hpack_decompress_state_cpp(DEFAULT_HEADER_TABLE_SIZE);
				REQUIRE_NE(decompress_state.get(), nullptr);

				const auto input = buffer_from_raw_data(test_case.raw_data);

				auto result = http2_hpack_decompress_data(decompress_state.get(), input);
				CppDefer<Http2HpackDecompressResult> defer = { &result,
					                                           free_hpack_decompress_result };

				std::string error = "";
				if(result.is_error) {
					error = std::string{ result.data.error };
				}

				INFO("Error occurred: ", error);
				REQUIRE_FALSE(result.is_error);

				const auto actual_result = result.data.result;

				const auto& expected_result = test_case.result;

				const auto actual_result_cpp = get_cpp_headers(actual_result);

				REQUIRE_EQ(actual_result_cpp, expected_result);

				const auto& expected_dynamic_table = test_case.dynamic_table;

				const auto actual_dynamic_table = get_dynamic_decompress_table(decompress_state);

				REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
			}();
		}
	}
}

namespace {

using HpackManualDeserializeTestCaseEntry = HeaderFieldDeserializeTest;

struct HpackManualTestDeserializeCase {
	std::string name;
	std::string description;
	size_t header_table_size;
	std::vector<HpackManualDeserializeTestCaseEntry> cases;
};

} // namespace

TEST_CASE("testing hpack deserializing - manual tests <hpack_deserialize_manual>") {

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	const std::vector<HpackManualTestDeserializeCase> test_cases = {
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3
		HpackManualTestDeserializeCase{
			.name  = "c.3",
			.description = "Request Examples without Huffman Coding",
			.header_table_size = DEFAULT_HEADER_TABLE_SIZE,
			.cases = std::vector<HpackManualDeserializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.1
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("828684410f7777772e6578616d706c652e636f6d"),
					.dynamic_table = {
						.entries = { {":authority", "www.example.com"}},
						.size  =  57,
					},
					.result = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("828684be58086e6f2d6361636865"),
					.dynamic_table = {
						.entries = { 
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  110,
					},
					.result = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
						{"cache-control","no-cache", },
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("828785bf400a637573746f6d2d6b65790c637573746f6d2d76616c7565"),
					.dynamic_table = {
						.entries = { 
							{"custom-key", "custom-value", },
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  164,
					},
					.result = {
						{":method","GET", },
						{":scheme","https", },
						{":path","/index.html", },
						{":authority","www.example.com", },
						{"custom-key", "custom-value", },
					}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4
		HpackManualTestDeserializeCase{
			.name  = "c.4",
			.description = "Request Examples with Huffman Coding",
			.header_table_size = DEFAULT_HEADER_TABLE_SIZE,
			.cases = std::vector<HpackManualDeserializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.1
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("828684418cf1e3c2e5f23a6ba0ab90f4ff"),
					.dynamic_table = {
						.entries = { {":authority", "www.example.com"}},
						.size  =  57,
					},
					.result = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("828684be5886a8eb10649cbf"),
					.dynamic_table = {
						.entries = { 
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  110,
					},
					.result = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
						{"cache-control","no-cache", },
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("828785bf408825a849e95ba97d7f8925a849e95bb8e8b4bf"),
					.dynamic_table = {
						.entries = { 
							{"custom-key", "custom-value", },
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  164,
					},
					.result = {
						{":method","GET", },
						{":scheme","https", },
						{":path","/index.html", },
						{":authority","www.example.com", },
						{"custom-key", "custom-value", },
					}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5
		HpackManualTestDeserializeCase{
			.name  = "c.5",
			.description = "Response Examples without Huffman Coding",
			.header_table_size = 256,
			.cases = std::vector<HpackManualDeserializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.1
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("4803333032580770726976617465611d4d6f6e2c203231204f637420323031332032303a31333a323120474d546e1768747470733a2f2f7777772e6578616d706c652e636f6d"),
					.dynamic_table = {
						.entries = { 
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
							{":status", "302"},
						},
						.size  =  222,
					},
					.result = {
						{":status", "302"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("4803333037c1c0bf"),
					.dynamic_table = {
						.entries = { 
							{":status", "307"},
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
						},
						.size  =  222,
					},
					.result = {
						{":status", "307"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("88c1611d4d6f6e2c203231204f637420323031332032303a31333a323220474d54c05a04677a69707738666f6f3d4153444a4b48514b425a584f5157454f50495541585157454f49553b206d61782d6167653d333630303b2076657273696f6e3d31"),
					.dynamic_table = {
						.entries = { 
							{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
							{"content-encoding", "gzip"},
							{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						},
						.size  =  215,
					},
					.result = {
						{":status", "200"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						{"location", "https://www.example.com"},
						{"content-encoding", "gzip"},
						{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
					}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6
		HpackManualTestDeserializeCase{
			.name  = "c.6",
			.description = "Response Examples with Huffman Coding",
			.header_table_size = 256,
			.cases = std::vector<HpackManualDeserializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.1
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("488264025885aec3771a4b6196d07abe941054d444a8200595040b8166e082a62d1bff6e919d29ad171863c78f0b97c8e9ae82ae43d3"),
					.dynamic_table = {
						.entries = { 
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
							{":status", "302"},
						},
						.size  =  222,
					},
					.result = {
						{":status", "302"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("4883640effc1c0bf"),
					.dynamic_table = {
						.entries = { 
							{":status", "307"},
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
						},
						.size  =  222,
					},
					.result = {
						{":status", "307"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = parse_wire_data("88c16196d07abe941054d444a8200595040b8166e084a62d1bffc05a839bd9ab77ad94e7821dd7f2e6c7b335dfdfcd5b3960d5af27087f3672c1ab270fb5291f9587316065c003ed4ee5b1063d5007"),
					.dynamic_table = {
						.entries = { 
							{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
							{"content-encoding", "gzip"},
							{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						},
						.size  =  215,
					},
					.result = {
						{":status", "200"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						{"location", "https://www.example.com"},
						{"content-encoding", "gzip"},
						{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
					}
				},
			}
		}
	};

	for(const auto& test_case : test_cases) {

		const auto case_str = std::string{ "Subcase " } + test_case.name;
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
				HpackDecompressStateCpp decompress_state =
				    get_default_hpack_decompress_state_cpp(test_case.header_table_size);
				REQUIRE_NE(decompress_state.get(), nullptr);

				for(size_t i = 0; i < test_case.cases.size(); ++i) {

					const auto& subcase = test_case.cases.at(i);

					const auto input = buffer_from_raw_data(subcase.raw_data);

					auto result = http2_hpack_decompress_data(decompress_state.get(), input);
					CppDefer<Http2HpackDecompressResult> defer = { &result,
						                                           free_hpack_decompress_result };

					std::string error = "";
					if(result.is_error) {
						error = std::string{ result.data.error };
					}

					INFO("request number: ", i);
					INFO("Error occurred: ", error);
					REQUIRE_FALSE(result.is_error);

					const auto actual_result = result.data.result;

					const auto& expected_result = subcase.result;

					const auto actual_result_cpp = get_cpp_headers(actual_result);

					REQUIRE_EQ(actual_result_cpp, expected_result);

					const auto& expected_dynamic_table = subcase.dynamic_table;

					const auto actual_dynamic_table =
					    get_dynamic_decompress_table(decompress_state);

					REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
				}
			}();
		}
	}
}

namespace {

struct HpackManualSerializeTestCaseEntry {
	std::vector<std::uint8_t> result;
	test::DynamicTable dynamic_table;
	std::vector<std::pair<std::string, std::string>> input;
	Http2HpackCompressOptions options;
};

struct HpackManualTestSerializeCase {
	std::string name;
	std::string description;
	size_t header_table_size;
	std::vector<HpackManualSerializeTestCaseEntry> cases;
};

} // namespace

[[nodiscard]] static std::vector<std::uint8_t> raw_data_from_buffer(const SizedBuffer& buffer) {

	std::vector<std::uint8_t> result{};
	result.reserve(buffer.size);

	for(size_t i = 0; i < buffer.size; ++i) {
		result.push_back(((uint8_t*)buffer.data)[i]);
	}

	return result;
}

TEST_CASE("testing hpack serializing - manual tests <hpack_serialize_manual>") {

	const auto hpack_cpp_global_handle = HpackGlobalHandle();

	const std::vector<HpackManualTestSerializeCase> test_cases = {
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3
		HpackManualTestSerializeCase{
			.name  = "c.3",
			.description = "Request Examples without Huffman Coding",
			.header_table_size = DEFAULT_HEADER_TABLE_SIZE,
			.cases = std::vector<HpackManualSerializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.1
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("828684410f7777772e6578616d706c652e636f6d"),
					.dynamic_table = {
						.entries = { {":authority", "www.example.com"}},
						.size  =  57,
					},
					.input = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageNever,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.2
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("828684be58086e6f2d6361636865"),
					.dynamic_table = {
						.entries = { 
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  110,
					},
					.input = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
						{"cache-control","no-cache", },
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageNever,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.3
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("828785bf400a637573746f6d2d6b65790c637573746f6d2d76616c7565"),
					.dynamic_table = {
						.entries = { 
							{"custom-key", "custom-value", },
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  164,
					},
					.input = {
						{":method","GET", },
						{":scheme","https", },
						{":path","/index.html", },
						{":authority","www.example.com", },
						{"custom-key", "custom-value", },
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageNever,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4
		HpackManualTestSerializeCase{
			.name  = "c.4",
			.description = "Request Examples with Huffman Coding",
			.header_table_size = DEFAULT_HEADER_TABLE_SIZE,
			.cases = std::vector<HpackManualSerializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.1
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("828684418cf1e3c2e5f23a6ba0ab90f4ff"),
					.dynamic_table = {
						.entries = { {":authority", "www.example.com"}},
						.size  =  57,
					},
					.input = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageAlways,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.2
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("828684be5886a8eb10649cbf"),
					.dynamic_table = {
						.entries = { 
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  110,
					},
					.input = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
						{"cache-control","no-cache", },
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageAlways,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.3
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("828785bf408825a849e95ba97d7f8925a849e95bb8e8b4bf"),
					.dynamic_table = {
						.entries = { 
							{"custom-key", "custom-value", },
							{"cache-control","no-cache"},
							{":authority", "www.example.com"}
						},
						.size  =  164,
					},
					.input = {
						{":method","GET", },
						{":scheme","https", },
						{":path","/index.html", },
						{":authority","www.example.com", },
						{"custom-key", "custom-value", },
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageAlways,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5
		HpackManualTestSerializeCase{
			.name  = "c.5",
			.description = "Response Examples without Huffman Coding",
			.header_table_size = 256,
			.cases = std::vector<HpackManualSerializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.1
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("4803333032580770726976617465611d4d6f6e2c203231204f637420323031332032303a31333a323120474d546e1768747470733a2f2f7777772e6578616d706c652e636f6d"),
					.dynamic_table = {
						.entries = { 
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
							{":status", "302"},
						},
						.size  =  222,
					},
					.input = {
						{":status", "302"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageNever,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.2
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("4803333037c1c0bf"),
					.dynamic_table = {
						.entries = { 
							{":status", "307"},
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
						},
						.size  =  222,
					},
					.input = {
						{":status", "307"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageNever,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.3
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("88c1611d4d6f6e2c203231204f637420323031332032303a31333a323220474d54c05a04677a69707738666f6f3d4153444a4b48514b425a584f5157454f50495541585157454f49553b206d61782d6167653d333630303b2076657273696f6e3d31"),
					.dynamic_table = {
						.entries = { 
							{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
							{"content-encoding", "gzip"},
							{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						},
						.size  =  215,
					},
					.input = {
						{":status", "200"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						{"location", "https://www.example.com"},
						{"content-encoding", "gzip"},
						{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageNever,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6
		HpackManualTestSerializeCase{
			.name  = "c.6",
			.description = "Response Examples with Huffman Coding",
			.header_table_size = 256,
			.cases = std::vector<HpackManualSerializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.1
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("488264025885aec3771a4b6196d07abe941054d444a8200595040b8166e082a62d1bff6e919d29ad171863c78f0b97c8e9ae82ae43d3"),
					.dynamic_table = {
						.entries = { 
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
							{":status", "302"},
						},
						.size  =  222,
					},
					.input = {
						{":status", "302"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageAlways,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.2
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("4883640effc1c0bf"),
					.dynamic_table = {
						.entries = { 
							{":status", "307"},
							{"location", "https://www.example.com"},
							{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
							{"cache-control", "private"},
						},
						.size  =  222,
					},
					.input = {
						{":status", "307"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
						{"location", "https://www.example.com"},
					},
					.options = {
						.huffman_usage = Http2HpackHuffmanUsageAlways,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.3
				HpackManualSerializeTestCaseEntry{ 
					.result = parse_wire_data("88c16196d07abe941054d444a8200595040b8166e084a62d1bffc05a839bd9ab77ad94e7821dd7f2e6c7b335dfdfcd5b3960d5af27087f3672c1ab270fb5291f9587316065c003ed4ee5b1063d5007"),
					.dynamic_table = {
						.entries = { 
							{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
							{"content-encoding", "gzip"},
							{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						},
						.size  =  215,
					},
					.input = {
						{":status", "200"},
						{"cache-control", "private"},
						{"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
						{"location", "https://www.example.com"},
						{"content-encoding", "gzip"},
						{"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"},
					},
					.options  = {
						.huffman_usage = Http2HpackHuffmanUsageAlways,
						.type = Http2HpackCompressTypeAllTablesUsage,
						.table_add_type = Http2HpackTableAddTypeAll,
					}
				},
			}
		}
	};

	for(const auto& test_case : test_cases) {

		const auto case_str = std::string{ "Subcase " } + test_case.name;
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
				HpackCompressStateCpp compress_state =
				    get_default_hpack_compress_state_cpp(test_case.header_table_size);
				REQUIRE_NE(compress_state.get(), nullptr);

				for(size_t i = 0; i < test_case.cases.size(); ++i) {
					INFO("iteration: ", i);

					if(test_case.name == "c.5") {
						INFO("hello");
					}

					const auto& subcase = test_case.cases.at(i);

					const auto input = subcase.input;
					auto input_c = get_c_map_from_cpp(input);

					auto result = http2_hpack_compress_data(compress_state.get(), *input_c.get(),
					                                        subcase.options);
					CppDefer<SizedBuffer> defer = { &result, [](SizedBuffer* buf) -> void {
						                               free_sized_buffer(*buf);
						                           } };

					REQUIRE_NE(result.data, nullptr);

					const auto& expected_result = subcase.result;

					const auto actual_result = raw_data_from_buffer(result);

					REQUIRE_EQ(actual_result, expected_result);

					const auto& expected_dynamic_table = subcase.dynamic_table;

					const auto actual_dynamic_table = get_dynamic_compress_table(compress_state);

					REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
				}
			}();
		}
	}
}

namespace {
struct TestCaseStrCmp {
	std::string value;
	bool result;
};
} // namespace

TEST_CASE("testing fast string comparison <fast_string_cmp>") {

	std::vector<TestCaseStrCmp> test_cases = {
		TestCaseStrCmp{ .value = "", .result = false },
		TestCaseStrCmp{ .value = "hello world", .result = false },
		TestCaseStrCmp{ .value = "longer string", .result = false },
	};

	const auto test_data_strings = generated::c_test_fns::get_test_data_strings();

	for(const auto& val : test_data_strings) {
		test_cases.emplace_back(val, true);
	}

	for(const auto& test_case : test_cases) {

		const bool expected_result = test_case.result;
		const auto& str = test_case.value;

		const auto actual_result = generated_c_test_fns_fast_string_compare_test_data(
		    tstr_view{ str.c_str(), str.size() });

		REQUIRE_EQ(actual_result.found, expected_result);

		if(actual_result.found) {
			REQUIRE_GE(actual_result.index, 0);
			REQUIRE_LT(actual_result.index, test_data_strings.size());

			const std::string& actual_result_str = test_data_strings.at(actual_result.index);

			REQUIRE_EQ(actual_result_str, str);
		} else {
			REQUIRE_EQ(actual_result.index, 0);
		}
	}
}

TEST_SUITE_END();
