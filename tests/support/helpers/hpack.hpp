#pragma once

#include "./cpp_types.hpp"
#include <http/hpack.h>
#include <http/hpack_huffman.h>

#include <filesystem>
#include <optional>
#include <vector>

namespace tests {

struct ThirdPartyHpackTestCaseEntry {
	size_t seqno;
	std::vector<std::uint8_t> wire_data;
	std::vector<std::pair<std::string, std::string>> headers;
	std::optional<size_t>
	    header_table_size; // the header table size sent in SETTINGS_HEADER_TABLE_SIZE and ACKed
	                       // just before this case. The first case should contain this field. If
	                       // omitted, the default value, 4,096, is used.
	std::vector<std::string> strict_error_state; // strict key errors
};

struct HeaderTableMode {
	bool all_the_same;
	size_t table_size;
};

struct ThirdPartyHpackTestCase {
	std::string description;
	std::vector<ThirdPartyHpackTestCaseEntry> cases;
	std::string test_name;
	HeaderTableMode header_mode;
	std::filesystem::path file;
};

} // namespace tests

namespace hpack {

struct HpackGlobalHandle {

	HpackGlobalHandle();

	HpackGlobalHandle(HpackGlobalHandle&&) = delete;

	HpackGlobalHandle(const HpackGlobalHandle&) = delete;

	HpackGlobalHandle& operator=(const HpackGlobalHandle&) = delete;

	HpackGlobalHandle operator=(HpackGlobalHandle&&) = delete;

	~HpackGlobalHandle() noexcept(false);
};

} // namespace hpack

namespace hpack::hacky_trick {

// NOTE: this is not usable from multiple threads, as the underlying thing is not synchronized and
// dependend o a static "instance" like global variable!

struct HpackDecodingErrorStateHack {
	HpackDecodingErrorStateHack();

	HpackDecodingErrorStateHack(HpackDecodingErrorStateHack&&) = delete;

	HpackDecodingErrorStateHack(const HpackDecodingErrorStateHack&) = delete;

	HpackDecodingErrorStateHack& operator=(const HpackDecodingErrorStateHack&) = delete;

	HpackDecodingErrorStateHack operator=(HpackDecodingErrorStateHack&&) = delete;

	~HpackDecodingErrorStateHack() noexcept(false);

	[[nodiscard]] std::vector<std::string> get_errors() const;
};

} // namespace hpack::hacky_trick

namespace hpack::helpers {
[[nodiscard]] std::vector<std::uint8_t> parse_wire_data(const std::string& raw_wire);

} // namespace hpack::helpers

namespace consts {
struct StrictErrorException {
	std::string suite_name;
	std::string test_name;
	size_t seqno;
	std::string field_name;

	[[nodiscard]] bool operator==(const StrictErrorException& lhs) const;
};

extern const size_t default_header_table_size;

} // namespace consts

namespace helpers {

std::vector<consts::StrictErrorException> get_strict_error_state_exceptions();

[[nodiscard]] std::vector<std::pair<std::string, std::string>>
get_cpp_headers(const HttpHeaderFields& fields);

[[nodiscard]] CAutoFreePtr<HttpHeaderFields>
get_c_map_from_cpp(const std::vector<std::pair<std::string, std::string>>& map);
} // namespace helpers

namespace hpack {

using HpackDecompressStateCpp = CAutoFreePtr<HpackDecompressState>;

[[nodiscard]] HpackDecompressStateCpp
get_default_hpack_decompress_state_cpp(size_t max_dynamic_table_byte_size);

using HpackCompressStateCpp = CAutoFreePtr<HpackCompressState>;

[[nodiscard]] HpackCompressStateCpp
get_default_hpack_compress_state_cpp(size_t max_dynamic_table_byte_size);

void free_hpack_decompress_result(Http2HpackDecompressResult* result);

} // namespace hpack

namespace test {

struct DynamicTable {
	std::vector<std::pair<std::string, std::string>> entries;
	size_t size;

	[[nodiscard]] bool operator==(const DynamicTable& table2) const;
};

} // namespace test

std::ostream& operator<<(std::ostream& os, const test::DynamicTable& table);

namespace hpack {

[[nodiscard]] test::DynamicTable get_dynamic_decompress_table(const HpackDecompressStateCpp& state);

[[nodiscard]] test::DynamicTable get_dynamic_compress_table(const HpackCompressStateCpp& state);
} // namespace hpack

namespace helpers {

template <typename T> struct OptionalOr {
  public:
	T value;
	OptionalOr(const T& val) : value{ val } {}

	friend std::ostream& operator<<(std::ostream& os, const OptionalOr<T>& val) {
		os << "OptionalOr{" << val.value << "}";
		return os;
	}

	[[nodiscard]] bool operator==(const std::optional<T>& lhs) const {
		if(!lhs.has_value()) {
			return true;
		}

		return this->value == lhs.value();
	}
};

struct GlobalHuffmanData {
	bool present;

	GlobalHuffmanData();

	~GlobalHuffmanData();
};

void free_huffman_decode_result(HuffmanDecodeResult* decode_result);

void free_huffman_encode_result(HuffmanEncodeResult* encode_result);

} // namespace helpers

namespace hpack::huffman {

[[nodiscard]] std::vector<std::uint8_t> all_values_vector();
} // namespace hpack::huffman

#include <deque>
#include <http/dynamic_hpack_table.h>

namespace hpack {

struct DynamicEntry {
	std::string key;
	std::string value;

	friend std::ostream& operator<<(std::ostream& os, const DynamicEntry& entry);

	[[nodiscard]] bool operator==(const DynamicEntry& lhs) const;
};

std::ostream& operator<<(std::ostream& os, const hpack::DynamicEntry& entry);

struct DynamicTableC {
  private:
	HpackHeaderDynamicTable m_table;

  public:
	DynamicTableC();

	DynamicTableC(DynamicTableC&&) = delete;

	DynamicTableC(const DynamicTableC&) = delete;

	DynamicTableC& operator=(const DynamicTableC&) = delete;

	DynamicTableC operator=(DynamicTableC&&) = delete;

	~DynamicTableC();

	[[nodiscard]] DynamicEntry operator[](size_t idx) const;

	[[nodiscard]] size_t size() const;

	[[nodiscard]] std::optional<DynamicEntry> pop_at_end();

	[[nodiscard]] bool insert_at_start(const DynamicEntry& entry);

	friend std::ostream& operator<<(std::ostream& os, const DynamicTableC& table);

	[[nodiscard]] bool operator==(const DynamicTableC& lhs) const;

	[[nodiscard]] bool operator==(const std::vector<DynamicEntry>& lhs) const;
};

std::ostream& operator<<(std::ostream& os, const hpack::DynamicTableC& table);

struct DynamicTableCpp {
  private:
	std::deque<DynamicEntry> m_deque;

  public:
	DynamicTableCpp();

	DynamicTableCpp(DynamicTableCpp&&) = delete;

	DynamicTableCpp(const DynamicTableCpp&) = delete;

	DynamicTableCpp& operator=(const DynamicTableCpp&) = delete;

	DynamicTableCpp operator=(DynamicTableCpp&&) = delete;

	~DynamicTableCpp();

	[[nodiscard]] DynamicEntry operator[](size_t idx) const;

	[[nodiscard]] size_t size() const;

	[[nodiscard]] std::optional<DynamicEntry> pop_at_end();

	[[nodiscard]] bool insert_at_start(const DynamicEntry& entry);

	friend std::ostream& operator<<(std::ostream& os, const DynamicTableCpp& table);

	[[nodiscard]] bool operator==(const DynamicTableCpp& lhs) const;

	[[nodiscard]] bool operator==(const std::vector<DynamicEntry>& lhs) const;
};

std::ostream& operator<<(std::ostream& os, const hpack::DynamicTableCpp& table);

[[nodiscard]] bool operator==(const DynamicTableC& rhs, const DynamicTableCpp& lhs);

[[nodiscard]] bool operator==(const DynamicTableCpp& rhs, const DynamicTableC& lhs);

} // namespace hpack
