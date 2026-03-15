
#include <doctest.h>

#include "generated_hpack_tests.hpp"

#include <support/helpers.hpp>
#include <support/helpers/hpack.hpp>

#include "helpers/json.hpp"
#include "helpers/string_maker.hpp"

TEST_SUITE_BEGIN("hpack/manual" * doctest::description("manual hpack tests") *
                 doctest::timeout(2.0));

struct IntegerTest {
	std::vector<std::uint8_t> values;
	std::uint8_t prefix_bits;
	HpackVariableInteger result;
};

TEST_CASE("testing hpack deserializing - integer tests <hpack_integer_deserialize>") {

	const auto hpack_cpp_global_handle = hpack::HpackGlobalHandle();

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

				REQUIRE_IS_NOT_ERROR(result);

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

			REQUIRE_IS_NOT_ERROR(result);

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

struct HeaderFieldDeserializeTest {
	std::vector<std::uint8_t> raw_data;
	test::DynamicTable dynamic_table;
	std::vector<std::pair<std::string, std::string>> result;
	std::vector<std::string> strict_error_state;
};

TEST_CASE("testing hpack deserializing - header field tests <hpack_header_fields>") {

	const auto hpack_cpp_global_handle = hpack::HpackGlobalHandle();

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.2
	const std::vector<HeaderFieldDeserializeTest> test_cases = {
		{ 
			.raw_data =hpack::helpers::parse_wire_data("400a637573746f6d2d6b65790d637573746f6d2d686561646572"),
			.dynamic_table = {
				.entries = { {"custom-key" , "custom-header"}},
				.size  =  55,
			},
			.result = {
				{"custom-key" , "custom-header"}
			},
			.strict_error_state = {}
		},
		{ 
			.raw_data =hpack::helpers::parse_wire_data("040c2f73616d706c652f70617468"),
			.dynamic_table = {
				.entries = { },
				.size  =  0,
			},
			.result = {
				{":path","/sample/path"}
			},
			.strict_error_state = {}
		},
		{ 
			.raw_data =hpack::helpers::parse_wire_data("100870617373776f726406736563726574"),
			.dynamic_table = {
				.entries = { },
				.size  =  0,
			},
			.result = {
				{"password","secret"}
			},
			.strict_error_state = {}
		},
		{ 
			.raw_data =hpack::helpers::parse_wire_data("82"),
			.dynamic_table = {
				.entries = { },
				.size  =  0,
			},
			.result = {
				{":method","GET"}
			},
			.strict_error_state = {}
		}
	};

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
				hpack::HpackDecompressStateCpp decompress_state =
				    hpack::get_default_hpack_decompress_state_cpp(
				        consts::default_header_table_size);
				REQUIRE_NE(decompress_state.get(), nullptr);

				const auto input = buffer_from_raw_data(test_case.raw_data);

				hpack::hacky_trick::HpackDecodingErrorStateHack error_state_stack{};

				auto result = http2_hpack_decompress_data(decompress_state.get(), input);
				CAutoFreePtr<Http2HpackDecompressResult> defer = {
					&result, hpack::free_hpack_decompress_result
				};

				REQUIRE_IS_NOT_ERROR(result);

				const auto actual_result = result.data.result;

				const auto& expected_result = test_case.result;

				const auto actual_result_cpp = helpers::get_cpp_headers(actual_result);

				REQUIRE_EQ(actual_result_cpp, expected_result);

				REQUIRE_EQ(error_state_stack.get_errors(), test_case.strict_error_state);

				const auto& expected_dynamic_table = test_case.dynamic_table;

				const auto actual_dynamic_table =
				    hpack::get_dynamic_decompress_table(decompress_state);

				REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
			}();
		}
	}
}

using HpackManualDeserializeTestCaseEntry = HeaderFieldDeserializeTest;

struct HpackManualTestDeserializeCase {
	std::string name;
	std::string description;
	size_t header_table_size;
	std::vector<HpackManualDeserializeTestCaseEntry> cases;
};

TEST_CASE("testing hpack deserializing - manual tests <hpack_deserialize_manual>") {

	const auto hpack_cpp_global_handle = hpack::HpackGlobalHandle();

	const std::vector<HpackManualTestDeserializeCase> test_cases = {
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3
		HpackManualTestDeserializeCase{
			.name  = "c.3",
			.description = "Request Examples without Huffman Coding",
			.header_table_size = consts::default_header_table_size,
			.cases = std::vector<HpackManualDeserializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.1
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("828684410f7777772e6578616d706c652e636f6d"),
					.dynamic_table = {
						.entries = { {":authority", "www.example.com"}},
						.size  =  57,
					},
					.result = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("828684be58086e6f2d6361636865"),
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
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("828785bf400a637573746f6d2d6b65790c637573746f6d2d76616c7565"),
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
					},
					.strict_error_state = {}
				},
			}
		},
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4
		HpackManualTestDeserializeCase{
			.name  = "c.4",
			.description = "Request Examples with Huffman Coding",
			.header_table_size = consts::default_header_table_size,
			.cases = std::vector<HpackManualDeserializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.1
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("828684418cf1e3c2e5f23a6ba0ab90f4ff"),
					.dynamic_table = {
						.entries = { {":authority", "www.example.com"}},
						.size  =  57,
					},
					.result = {
						{":method","GET", },
						{":scheme","http", },
						{":path","/", },
						{":authority","www.example.com", },
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("828684be5886a8eb10649cbf"),
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
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("828785bf408825a849e95ba97d7f8925a849e95bb8e8b4bf"),
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
					},
					.strict_error_state = {}
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
					.raw_data = hpack::helpers::parse_wire_data("4803333032580770726976617465611d4d6f6e2c203231204f637420323031332032303a31333a323120474d546e1768747470733a2f2f7777772e6578616d706c652e636f6d"),
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
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("4803333037c1c0bf"),
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
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.5.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("88c1611d4d6f6e2c203231204f637420323031332032303a31333a323220474d54c05a04677a69707738666f6f3d4153444a4b48514b425a584f5157454f50495541585157454f49553b206d61782d6167653d333630303b2076657273696f6e3d31"),
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
					},
					.strict_error_state = {}
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
					.raw_data = hpack::helpers::parse_wire_data("488264025885aec3771a4b6196d07abe941054d444a8200595040b8166e082a62d1bff6e919d29ad171863c78f0b97c8e9ae82ae43d3"),
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
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.2
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("4883640effc1c0bf"),
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
					},
					.strict_error_state = {}
				},
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.6.3
				HpackManualDeserializeTestCaseEntry{ 
					.raw_data = hpack::helpers::parse_wire_data("88c16196d07abe941054d444a8200595040b8166e084a62d1bffc05a839bd9ab77ad94e7821dd7f2e6c7b335dfdfcd5b3960d5af27087f3672c1ab270fb5291f9587316065c003ed4ee5b1063d5007"),
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
					},
					.strict_error_state = {}
				},
			}
		}
	};

	for(const auto& test_case : test_cases) {

		const auto case_str = std::string{ "Subcase " } + test_case.name;
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
				hpack::HpackDecompressStateCpp decompress_state =
				    hpack::get_default_hpack_decompress_state_cpp(test_case.header_table_size);
				REQUIRE_NE(decompress_state.get(), nullptr);

				for(size_t i = 0; i < test_case.cases.size(); ++i) {
					INFO("request number: ", i);

					const auto& subcase = test_case.cases.at(i);

					const auto input = buffer_from_raw_data(subcase.raw_data);

					hpack::hacky_trick::HpackDecodingErrorStateHack error_state_stack{};

					auto result = http2_hpack_decompress_data(decompress_state.get(), input);
					CAutoFreePtr<Http2HpackDecompressResult> defer = {
						&result, hpack::free_hpack_decompress_result
					};

					REQUIRE_IS_NOT_ERROR(result);

					const auto actual_result = result.data.result;

					const auto& expected_result = subcase.result;

					const auto actual_result_cpp = helpers::get_cpp_headers(actual_result);

					REQUIRE_EQ(actual_result_cpp, expected_result);

					REQUIRE_EQ(error_state_stack.get_errors(), subcase.strict_error_state);

					const auto& expected_dynamic_table = subcase.dynamic_table;

					const auto actual_dynamic_table =
					    hpack::get_dynamic_decompress_table(decompress_state);

					REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
				}
			}();
		}
	}
}

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

TEST_CASE("testing hpack serializing - manual tests <hpack_serialize_manual>") {

	const auto hpack_cpp_global_handle = hpack::HpackGlobalHandle();

	const std::vector<HpackManualTestSerializeCase> test_cases = {
		// see https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3
		HpackManualTestSerializeCase{
			.name  = "c.3",
			.description = "Request Examples without Huffman Coding",
			.header_table_size = consts::default_header_table_size,
			.cases = std::vector<HpackManualSerializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.3.1
				HpackManualSerializeTestCaseEntry{ 
					.result = hpack::helpers::parse_wire_data("828684410f7777772e6578616d706c652e636f6d"),
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
					.result = hpack::helpers::parse_wire_data("828684be58086e6f2d6361636865"),
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
					.result = hpack::helpers::parse_wire_data("828785bf400a637573746f6d2d6b65790c637573746f6d2d76616c7565"),
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
			.header_table_size = consts::default_header_table_size,
			.cases = std::vector<HpackManualSerializeTestCaseEntry>{
				// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4.1
				HpackManualSerializeTestCaseEntry{ 
					.result = hpack::helpers::parse_wire_data("828684418cf1e3c2e5f23a6ba0ab90f4ff"),
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
					.result = hpack::helpers::parse_wire_data("828684be5886a8eb10649cbf"),
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
					.result = hpack::helpers::parse_wire_data("828785bf408825a849e95ba97d7f8925a849e95bb8e8b4bf"),
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
					.result = hpack::helpers::parse_wire_data("4803333032580770726976617465611d4d6f6e2c203231204f637420323031332032303a31333a323120474d546e1768747470733a2f2f7777772e6578616d706c652e636f6d"),
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
					.result = hpack::helpers::parse_wire_data("4803333037c1c0bf"),
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
					.result = hpack::helpers::parse_wire_data("88c1611d4d6f6e2c203231204f637420323031332032303a31333a323220474d54c05a04677a69707738666f6f3d4153444a4b48514b425a584f5157454f50495541585157454f49553b206d61782d6167653d333630303b2076657273696f6e3d31"),
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
					.result = hpack::helpers::parse_wire_data("488264025885aec3771a4b6196d07abe941054d444a8200595040b8166e082a62d1bff6e919d29ad171863c78f0b97c8e9ae82ae43d3"),
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
					.result = hpack::helpers::parse_wire_data("4883640effc1c0bf"),
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
					.result = hpack::helpers::parse_wire_data("88c16196d07abe941054d444a8200595040b8166e084a62d1bffc05a839bd9ab77ad94e7821dd7f2e6c7b335dfdfcd5b3960d5af27087f3672c1ab270fb5291f9587316065c003ed4ee5b1063d5007"),
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
				hpack::HpackCompressStateCpp compress_state =
				    hpack::get_default_hpack_compress_state_cpp(test_case.header_table_size);
				REQUIRE_NE(compress_state.get(), nullptr);

				for(size_t i = 0; i < test_case.cases.size(); ++i) {
					INFO("iteration: ", i);

					if(test_case.name == "c.5") {
						INFO("hello");
					}

					const auto& subcase = test_case.cases.at(i);

					const auto input = subcase.input;
					auto input_c = helpers::get_c_map_from_cpp(input);

					auto result = http2_hpack_compress_data(compress_state.get(), *input_c.get(),
					                                        subcase.options);
					CAutoFreePtr<SizedBuffer> defer = { &result, [](SizedBuffer* buf) -> void {
						                                   free_sized_buffer(*buf);
						                               } };

					REQUIRE_NE(result.data, nullptr);

					const auto& expected_result = subcase.result;

					const auto actual_result = helpers::raw_data_from_buffer(result);

					REQUIRE_EQ(actual_result, expected_result);

					const auto& expected_dynamic_table = subcase.dynamic_table;

					const auto actual_dynamic_table =
					    hpack::get_dynamic_compress_table(compress_state);

					REQUIRE_EQ(expected_dynamic_table, actual_dynamic_table);
				}
			}();
		}
	}
}

struct TestCaseStrCmp {
	std::string value;
	bool result;
};

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

namespace dynamic_op {

enum class IsEmptyType : std::uint8_t {
	PopAtEnd,
	IndexThrows,
	SizeIsZero,
};

struct IsEmpty {
	IsEmptyType type;
};

struct InsertEntry {
	hpack::DynamicEntry entry_to_insert;
};

struct RemoveEntry {
	hpack::DynamicEntry remove_result;
};

using DynamicTableOperation = std::variant<IsEmpty, InsertEntry, RemoveEntry>;

static DynamicTableOperation insert_at_start(hpack::DynamicEntry&& value) {
	return DynamicTableOperation{ InsertEntry{ std::move(value) } };
}

static DynamicTableOperation remove_at_end(hpack::DynamicEntry&& value) {
	return DynamicTableOperation{ RemoveEntry{ std::move(value) } };
}

static DynamicTableOperation remove_at_end() {
	return DynamicTableOperation{ IsEmpty{ IsEmptyType::PopAtEnd } };
}

static DynamicTableOperation is_empty(IsEmptyType type = IsEmptyType::SizeIsZero) {
	return DynamicTableOperation{ IsEmpty{ type } };
}
} // namespace dynamic_op

namespace helper {

template <class... Ts> struct Overloaded : Ts... {
	using Ts::operator()...;
};
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;
} // namespace helper

struct DynamicTableOperationsTest {
	std::string description;
	std::vector<dynamic_op::DynamicTableOperation> operations;
	std::vector<hpack::DynamicEntry> result;
};

TEST_CASE("testing dynamic hpack table <dynamic_hpack_table>") {

	std::vector<DynamicTableOperationsTest> test_cases = {
		DynamicTableOperationsTest{
		    .description = "nothing to remove at the start",
		    .operations =
		        std::vector<dynamic_op::DynamicTableOperation>{
		            dynamic_op::remove_at_end(),
		        },
		    .result = std::vector<hpack::DynamicEntry>{},
		},
		DynamicTableOperationsTest{
		    .description = "is empty at the start (size is 0)",
		    .operations =
		        std::vector<dynamic_op::DynamicTableOperation>{
		            dynamic_op::is_empty(dynamic_op::IsEmptyType::SizeIsZero),
		        },
		    .result = std::vector<hpack::DynamicEntry>{},
		},
		DynamicTableOperationsTest{
		    .description = "is empty at the start (index throws)",
		    .operations =
		        std::vector<dynamic_op::DynamicTableOperation>{
		            dynamic_op::is_empty(dynamic_op::IsEmptyType::IndexThrows),
		        },
		    .result = std::vector<hpack::DynamicEntry>{},
		},
		DynamicTableOperationsTest{
		    .description = "some insertions, no removals",
		    .operations =
		        std::vector<dynamic_op::DynamicTableOperation>{
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key1", .value = "value1" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key2", .value = "value2" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key3", .value = "value3" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key4", .value = "value4" }) },
		    .result =
		        std::vector<hpack::DynamicEntry>{
		            hpack::DynamicEntry{ .key = "test_key4", .value = "value4" },
		            hpack::DynamicEntry{ .key = "test_key3", .value = "value3" },
		            hpack::DynamicEntry{ .key = "test_key2", .value = "value2" },
		            hpack::DynamicEntry{ .key = "test_key1", .value = "value1" },
		        },
		},
		DynamicTableOperationsTest{
		    .description = "some insertions, some removals (not all)",
		    .operations =
		        std::vector<dynamic_op::DynamicTableOperation>{
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key1", .value = "value1" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key2", .value = "value2" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key3", .value = "value3" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key4", .value = "value4" }),
		            //
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key1", .value = "value1" }),
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key2", .value = "value2" }),
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key3", .value = "value3" }),
		        },
		    .result = std::vector<hpack::DynamicEntry>{ hpack::DynamicEntry{ .key = "test_key4",
		                                                                     .value = "value4" } },
		},
		DynamicTableOperationsTest{
		    .description = "some insertions, all get removed",
		    .operations =
		        std::vector<dynamic_op::DynamicTableOperation>{
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key1", .value = "value1" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key2", .value = "value2" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key3", .value = "value3" }),
		            dynamic_op::insert_at_start(
		                hpack::DynamicEntry{ .key = "test_key4", .value = "value4" }),
		            //
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key1", .value = "value1" }),
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key2", .value = "value2" }),
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key3", .value = "value3" }),
		            dynamic_op::remove_at_end(
		                hpack::DynamicEntry{ .key = "test_key4", .value = "value4" }),
		            dynamic_op::is_empty() },
		    .result = std::vector<hpack::DynamicEntry>{},
		},

	};

	// generate some tests

	{ // test 1
		std::vector<dynamic_op::DynamicTableOperation> operations = { dynamic_op::is_empty() };

		const size_t amount = 1024;

		auto get_entry = [](size_t i) -> hpack::DynamicEntry {
			return hpack::DynamicEntry{ .key = std::string{ "test_key_" } + std::to_string(i),
				                        .value = std::string{ "test_value_" } + std::to_string(i) };
		};

		for(size_t i = 0; i < amount; ++i) {
			operations.emplace_back(dynamic_op::insert_at_start(get_entry(i)));
		}

		for(size_t i = 0; i < amount; ++i) {
			operations.emplace_back(dynamic_op::remove_at_end(get_entry(i)));
		}

		operations.emplace_back(dynamic_op::is_empty());

		std::vector<hpack::DynamicEntry> result = {};

		test_cases.emplace_back("many insertions and the same amount of removals",
		                        std::move(operations), std::move(result));
	}

	for(size_t i = 0; i < test_cases.size(); ++i) {

		const auto& test_case = test_cases.at(i);

		const auto case_str = std::string{ "Subcase " } + std::to_string(i);
		doctest::String case_name = doctest::String{ case_str.c_str() };

		SUBCASE(case_name) {
			[&test_case]() -> void {
				INFO("test case description: ", test_case.description);

				hpack::DynamicTableC table_c = {};
				hpack::DynamicTableC table_cpp = {};

				for(const auto& op : test_case.operations) {

					std::visit(
					    helper::Overloaded{
					        [&table_c,
					         &table_cpp](const dynamic_op::InsertEntry& insert_entry) -> void {
						        const auto insert_c_result =
						            table_c.insert_at_start(insert_entry.entry_to_insert);

						        REQUIRE(insert_c_result);

						        const auto insert_cpp_result =
						            table_cpp.insert_at_start(insert_entry.entry_to_insert);

						        REQUIRE(insert_cpp_result);
					        },
					        [&table_c,
					         &table_cpp](const dynamic_op::RemoveEntry& remove_entry) -> void {
						        const auto remove_c_result = table_c.pop_at_end();

						        REQUIRE_NE(remove_c_result,
						                   std::optional<hpack::DynamicEntry>{ std::nullopt });

						        REQUIRE_EQ(remove_c_result, remove_entry.remove_result);

						        const auto remove_cpp_result = table_cpp.pop_at_end();

						        REQUIRE_NE(remove_cpp_result,
						                   std::optional<hpack::DynamicEntry>{ std::nullopt });

						        REQUIRE_EQ(remove_cpp_result, remove_entry.remove_result);

						        REQUIRE_EQ(remove_c_result, remove_cpp_result);
					        },
					        [&table_c, &table_cpp](const dynamic_op::IsEmpty& is_empty) -> void {
						        switch(is_empty.type) {
							        case dynamic_op::IsEmptyType::PopAtEnd: {

								        const auto remove_c_result = table_c.pop_at_end();

								        REQUIRE_EQ(
								            remove_c_result,
								            std::optional<hpack::DynamicEntry>{ std::nullopt });

								        const auto remove_cpp_result = table_cpp.pop_at_end();

								        REQUIRE_EQ(
								            remove_cpp_result,
								            std::optional<hpack::DynamicEntry>{ std::nullopt });

								        REQUIRE_EQ(remove_c_result, remove_cpp_result);

								        break;
							        }
							        case dynamic_op::IsEmptyType::IndexThrows: {

								        auto c_func = [&table_c]() -> void {
									        const auto _ = table_c[1];
								        };

								        auto cpp_func = [&table_cpp]() -> void {
									        const auto _ = table_cpp[1];
								        };

								        REQUIRE_THROWS_WITH_AS(c_func(), "index out of range",
								                               const std::out_of_range&);

								        REQUIRE_THROWS_WITH_AS(cpp_func(), "index out of range",
								                               const std::out_of_range&);

								        break;
							        }
							        case dynamic_op::IsEmptyType::SizeIsZero: {

								        const auto c_size = table_c.size();

								        REQUIRE_EQ(c_size, 0);

								        const auto cpp_size = table_cpp.size();

								        REQUIRE_EQ(cpp_size, 0);

								        REQUIRE_EQ(c_size, cpp_size);

								        break;
							        }
							        default: {
								        UNREACHABLE();
							        }

								        // use index operator!

								        return;
						        }
					        },
					    },
					    op);
				}

				REQUIRE_EQ(table_c, table_cpp);

				REQUIRE_EQ(table_c, test_case.result);

				REQUIRE_EQ(table_cpp, test_case.result);
			}();
		}
	}

	SUBCASE("capacity shrinking is implemented correctly") { // case two, not enough data
		[]() -> void {
			auto get_entry = [](size_t i) -> hpack::DynamicEntry {
				return hpack::DynamicEntry{ .key = std::string{ "test_key_" } + std::to_string(i),
					                        .value =
					                            std::string{ "test_value_" } + std::to_string(i) };
			};

			hpack::DynamicTableC table_c = {};
			hpack::DynamicTableC table_cpp = {};

			REQUIRE_EQ(table_c.capacity(), 0);

			const size_t amount = 1024;

			// insert many elements
			for(size_t i = 0; i < amount; ++i) {

				const auto& insert_entry = get_entry(i);

				const auto insert_c_result = table_c.insert_at_start(insert_entry);

				REQUIRE(insert_c_result);

				const auto insert_cpp_result = table_cpp.insert_at_start(insert_entry);

				REQUIRE(insert_cpp_result);
			}

			REQUIRE_GE(table_c.capacity(), amount);

			// remove all
			for(size_t i = 0; i < amount; ++i) {

				const auto& remove_result = get_entry(i);

				const auto remove_c_result = table_c.pop_at_end();

				REQUIRE_NE(remove_c_result, std::optional<hpack::DynamicEntry>{ std::nullopt });

				REQUIRE_EQ(remove_c_result, remove_result);

				const auto remove_cpp_result = table_cpp.pop_at_end();

				REQUIRE_NE(remove_cpp_result, std::optional<hpack::DynamicEntry>{ std::nullopt });

				REQUIRE_EQ(remove_cpp_result, remove_result);

				REQUIRE_EQ(remove_c_result, remove_cpp_result);
			}

			// should be empty
			{
				const auto c_size = table_c.size();

				REQUIRE_EQ(c_size, 0);

				const auto cpp_size = table_cpp.size();

				REQUIRE_EQ(cpp_size, 0);

				REQUIRE_EQ(c_size, cpp_size);
			}

			// should have shrunk capacity
			{
				REQUIRE_LT(table_c.capacity(), amount);

				// implementation defined, can change at any time
				REQUIRE_EQ(table_c.capacity(), 32);
			}
		}();
	}
}

TEST_SUITE_END();
