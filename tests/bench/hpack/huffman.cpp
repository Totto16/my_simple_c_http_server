#include <benchmark/benchmark.h>

#include <http/hpack_huffman.h>

#include "generated_hpack_tests.hpp"

#include <functional>
#include <memory>

[[nodiscard]] static SizedBuffer buffer_from_str(const std::string& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

[[nodiscard]] static tstr tstr_from_utf8_string(const std::vector<std::uint8_t>& val) {
	const tstr buffer = tstr_from_len((const char*)val.data(), val.size());
	return buffer;
}

[[nodiscard]] static inline tstr tstr_from_string(const std::string& value) {
	return tstr_from_len(value.c_str(), value.size());
}

[[nodiscard]] static inline SizedBuffer
buffer_from_raw_data(const std::vector<std::uint8_t>& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

template <typename T> struct CppDefer {
  public:
	using FreeFn = std::function<void(T*)>;

  private:
	T* m_state;
	FreeFn m_free_fn;

  public:
	CppDefer(T* state, const FreeFn& free_fn) : m_state{ state }, m_free_fn{ free_fn } {}

	CppDefer(CppDefer&&) = delete;

	CppDefer(const CppDefer&) = delete;

	CppDefer& operator=(const CppDefer&) = delete;

	CppDefer operator=(CppDefer&&) = delete;

	const T* const get() const { return this->m_state; }

	~CppDefer() { this->m_free_fn(this->m_state); }
};

NODISCARD static bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs) {

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

struct GlobalHuffmanData {
	bool present;

  public:
	GlobalHuffmanData() : present{ true } { global_initialize_http2_hpack_huffman_data(); }

	GlobalHuffmanData(GlobalHuffmanData&&) = delete;

	GlobalHuffmanData(const GlobalHuffmanData&) = delete;

	GlobalHuffmanData& operator=(const GlobalHuffmanData&) = delete;

	GlobalHuffmanData operator=(GlobalHuffmanData&&) = delete;

	~GlobalHuffmanData() { global_free_http2_hpack_huffman_data(); }
};

static GlobalHuffmanData g_global_huffman_data = {};

struct TestCaseManual {
	std::vector<std::uint8_t> encoded;
	std::string str;
};

static void free_huffman_decode_result(HuffmanDecodeResult* decode_result) {
	if(decode_result->is_error) {
		return;
	}

	free_sized_buffer(decode_result->data.result);
}

static void free_huffman_encode_result(HuffmanEncodeResult* encode_result) {
	if(encode_result->is_error) {
		return;
	}

	free_sized_buffer(encode_result->data.result);
}

static void BM_hpack_huffman_decode_spec(benchmark::State& state) {

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4
	const std::vector<TestCaseManual> test_cases = {
		TestCaseManual{
		    .encoded = { 0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff },
		    .str = "www.example.com" },
		TestCaseManual{ .encoded = { 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf }, .str = "no-cache" },
		TestCaseManual{ .encoded = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f },
		                .str = "custom-key" },
		TestCaseManual{ .encoded = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf },
		                .str = "custom-value" },
		TestCaseManual{ .encoded = { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44,
		                             0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
		                             0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff },
		                .str = "Mon, 21 Oct 2013 20:13:21 GMT" },
		TestCaseManual{ .encoded = { 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97,
		                             0xc8, 0xe9, 0xae, 0x82, 0xae, 0x43, 0xd3 },
		                .str = "https://www.example.com" },
		TestCaseManual{ .encoded = { 0x64, 0x0e, 0xff }, .str = "307" },
		TestCaseManual{ .encoded = { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44,
		                             0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
		                             0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff },
		                .str = "Mon, 21 Oct 2013 20:13:22 GMT" },
		TestCaseManual{ .encoded = { 0x9b, 0xd9, 0xab }, .str = "gzip" },
		TestCaseManual{ .encoded = { 0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3,
		                             0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60, 0xd5, 0xaf,
		                             0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f,
		                             0xb5, 0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0,
		                             0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07 },
		                .str = "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1" },
	};

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			const auto input = buffer_from_raw_data(test_case.encoded);

			auto result = hpack_huffman_decode_bytes(input);
			CppDefer<HuffmanDecodeResult> defer = { &result, free_huffman_decode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_str(test_case.str);

			const std::string actual_result_str =
			    std::string{ (char*)actual_result.data, actual_result.size };

			assert(actual_result == expected_result);
		}
	}
}

static void BM_hpack_huffman_decode_ascii_generated(benchmark::State& state) {

	const auto& test_cases = generated::tests::test_cases_ascii;

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			const auto input = buffer_from_raw_data(test_case.encoded);

			auto result = hpack_huffman_decode_bytes(input);
			CppDefer<HuffmanDecodeResult> defer = { &result, free_huffman_decode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_str(test_case.str);

			const std::string actual_result_str =
			    std::string{ (char*)actual_result.data, actual_result.size };

			assert(actual_result == expected_result);
		}
	}
}

static void BM_hpack_huffman_decode_utf8_generated(benchmark::State& state) {

	const auto& test_cases = generated::tests::test_cases_utf8;

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			const auto input = buffer_from_raw_data(test_case.encoded);

			auto result = hpack_huffman_decode_bytes(input);
			CppDefer<HuffmanDecodeResult> defer = { &result, free_huffman_decode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_raw_data(test_case.value);

			assert(actual_result == expected_result);
		}
	}
}

static void BM_hpack_huffman_encode_spec(benchmark::State& state) {

	// see: https://datatracker.ietf.org/doc/html/rfc7541#appendix-C.4
	const std::vector<TestCaseManual> test_cases = {
		TestCaseManual{
		    .encoded = { 0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff },
		    .str = "www.example.com" },
		TestCaseManual{ .encoded = { 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf }, .str = "no-cache" },
		TestCaseManual{ .encoded = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f },
		                .str = "custom-key" },
		TestCaseManual{ .encoded = { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf },
		                .str = "custom-value" },
		TestCaseManual{ .encoded = { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44,
		                             0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
		                             0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff },
		                .str = "Mon, 21 Oct 2013 20:13:21 GMT" },
		TestCaseManual{ .encoded = { 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97,
		                             0xc8, 0xe9, 0xae, 0x82, 0xae, 0x43, 0xd3 },
		                .str = "https://www.example.com" },
		TestCaseManual{ .encoded = { 0x64, 0x0e, 0xff }, .str = "307" },
		TestCaseManual{ .encoded = { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44,
		                             0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
		                             0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff },
		                .str = "Mon, 21 Oct 2013 20:13:22 GMT" },
		TestCaseManual{ .encoded = { 0x9b, 0xd9, 0xab }, .str = "gzip" },
		TestCaseManual{ .encoded = { 0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3,
		                             0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60, 0xd5, 0xaf,
		                             0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f,
		                             0xb5, 0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0,
		                             0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07 },
		                .str = "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1" },
	};

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			auto input = tstr_from_string(test_case.str);
			CppDefer<tstr> defer_tstr = { &input, tstr_free };

			auto result = hpack_huffman_encode_value(&input);
			CppDefer<HuffmanEncodeResult> defer = { &result, free_huffman_encode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_raw_data(test_case.encoded);

			assert(actual_result == expected_result);
		}
	}
}

static void BM_hpack_huffman_encode_ascii_generated(benchmark::State& state) {

	const auto& test_cases = generated::tests::test_cases_ascii;

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			auto input = tstr_from_string(test_case.str);
			CppDefer<tstr> defer_tstr = { &input, tstr_free };

			auto result = hpack_huffman_encode_value(&input);
			CppDefer<HuffmanEncodeResult> defer = { &result, free_huffman_encode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_raw_data(test_case.encoded);

			assert(actual_result == expected_result);
		}
	}
}

static void BM_hpack_huffman_encode_utf8_generated(benchmark::State& state) {

	const auto& test_cases = generated::tests::test_cases_utf8;

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			auto input = tstr_from_utf8_string(test_case.value);
			CppDefer<tstr> defer_tstr = { &input, tstr_free };

			auto result = hpack_huffman_encode_value(&input);
			CppDefer<HuffmanEncodeResult> defer = { &result, free_huffman_encode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto actual_result = result.data.result;

			const auto expected_result = buffer_from_raw_data(test_case.encoded);

			assert(actual_result == expected_result);
		}
	}
}

struct TestCaseExtended {
	std::vector<std::uint8_t> value;
};

[[nodiscard]] static std::vector<std::uint8_t> vector_from_string(const std::string& data) {
	std::vector<std::uint8_t> result = {};
	result.reserve(data.size());

	for(const auto& ch : data) {
		result.emplace_back(ch);
	}

	return result;
}

[[nodiscard]] static std::vector<std::uint8_t> all_values_vector() {
	std::vector<std::uint8_t> result = {};
	result.reserve(256);

	for(size_t i = 0; i < 256; ++i) {
		result.emplace_back((uint8_t)i);
	}

	return result;
}

static void BM_hpack_huffman_roundtrip(benchmark::State& state) {

	const std::vector<TestCaseExtended> test_cases = {
		TestCaseExtended{ .value = vector_from_string("test hello") },
		TestCaseExtended{ .value = vector_from_string("test hello a long non ascii string öäüß") },
		TestCaseExtended{ .value = all_values_vector() },
	};

	for(auto _ : state) {
		// This code gets timed

		for(size_t i = 0; i < test_cases.size(); ++i) {

			const auto& test_case = test_cases.at(i);

			assert(g_global_huffman_data.present == true);

			auto input = tstr_from_utf8_string(test_case.value);
			CppDefer<tstr> defer_tstr = { &input, tstr_free };

			auto result = hpack_huffman_encode_value(&input);
			CppDefer<HuffmanEncodeResult> defer = { &result, free_huffman_encode_result };

			const char* error = nullptr;

			if(result.is_error) {
				error = result.data.error;
			}

			assert(error == nullptr);

			const auto intermediary_result = result.data.result;

			auto result_dec = hpack_huffman_decode_bytes(intermediary_result);
			CppDefer<HuffmanDecodeResult> defer2 = { &result_dec, free_huffman_decode_result };

			const char* error2 = nullptr;

			if(result_dec.is_error) {
				error2 = result_dec.data.error;
			}

			assert(error2 == nullptr);

			const auto actual_result = result_dec.data.result;

			const auto expected_result = buffer_from_raw_data(test_case.value);

			assert(actual_result == expected_result);
		}
	}
}

BENCHMARK(BM_hpack_huffman_decode_spec)->Name("hpack/huffman/decode_spec");

BENCHMARK(BM_hpack_huffman_decode_ascii_generated)->Name("hpack/huffman/decode_ascii_generated");

BENCHMARK(BM_hpack_huffman_decode_utf8_generated)->Name("hpack/huffman/decode_utf8_generated");

BENCHMARK(BM_hpack_huffman_encode_spec)->Name("hpack/huffman/encode_spec");

BENCHMARK(BM_hpack_huffman_encode_ascii_generated)->Name("hpack/huffman/encode_ascii_generated");

BENCHMARK(BM_hpack_huffman_encode_utf8_generated)->Name("hpack/huffman/encode_utf8_generated");

BENCHMARK(BM_hpack_huffman_roundtrip)->Name("hpack/huffman/roundtrip");
