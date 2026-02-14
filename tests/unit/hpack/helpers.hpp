#pragma once

#include <http/hpack.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>


struct ThirdPartyHpackTestCaseEntry {
	size_t seqno;
	std::vector<std::uint8_t> wire_data;
	std::unordered_map<std::string, std::string> headers;
};

struct ThirdPartyHpackTestCase {
	std::string description;
	std::vector<ThirdPartyHpackTestCaseEntry> cases;
	std::string name;
};

struct HpackGlobalHandle {

	HpackGlobalHandle() { global_initialize_http2_hpack_data(); }

	HpackGlobalHandle(HpackGlobalHandle&&) = delete;

	HpackGlobalHandle(const HpackGlobalHandle&) = delete;

	HpackGlobalHandle& operator=(const HpackGlobalHandle&) = delete;

	HpackGlobalHandle operator=(HpackGlobalHandle&&) = delete;

	~HpackGlobalHandle() { global_free_http2_hpack_data(); }
};

[[nodiscard]] static SizedBuffer buffer_from_raw_data(const std::vector<std::uint8_t>& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

[[nodiscard]] static std::uint8_t parse_hex_byte(const char& val) {

	if(val >= '0' && val <= '9') {
		return val - '0';
	}

	if(val >= 'a' && val <= 'f') {
		return 10 + (val - 'a');
	}

	if(val >= 'A' && val <= 'F') {
		return 10 + (val - 'A');
	}

	throw std::runtime_error("invalid byte data");
}

[[nodiscard]] static std::vector<std::uint8_t> parse_wire_data(const std::string& raw_wire) {

	std::vector<std::uint8_t> result{};

	if(raw_wire.length() % 2 != 0) {
		throw std::runtime_error("invalid wire data");
	}

	for(size_t i = 0; i < raw_wire.length(); i += 2) {

		const auto first_byte = parse_hex_byte(raw_wire.at(i));
		const auto second_byte = parse_hex_byte(raw_wire.at(i + 1));

		result.push_back((first_byte << 4) + second_byte);
	}

	return result;
}

[[nodiscard]] static std::unordered_map<std::string, std::string>
parse_headers_map(const nlohmann::json& value) {

	std::unordered_map<std::string, std::string> result{};

	if(!value.is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(const auto& val : value) {

		if(!val.is_object()) {
			throw std::runtime_error("json is malformed");
		}

		for(auto& el : val.items()) {
			result.insert_or_assign(el.key(), el.value());
		}
	}

	return result;
}

[[nodiscard]] static ThirdPartyHpackTestCaseEntry get_case_from_json(const nlohmann::json& value) {

	size_t seqno = value["seqno"].get<size_t>();

	const std::string raw_wire_data = value["wire"].get<std::string>();

	const auto wire_data = parse_wire_data(raw_wire_data);

	const auto headers = parse_headers_map(value["headers"]);

	return ThirdPartyHpackTestCaseEntry{
		.seqno = seqno,
		.wire_data = wire_data,
		.headers = headers,
	};
}

[[nodiscard]] static ThirdPartyHpackTestCase
get_thirdparty_hpack_test_case(const std::filesystem::path& path) {

	std::ifstream file_stream{ path };
	nlohmann::json data = nlohmann::json::parse(file_stream);

	std::string description = data.at("description").get<std::string>();

	std::vector<ThirdPartyHpackTestCaseEntry> cases{};

	if(!data.contains("cases") || !data["cases"].is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(const auto& case_ : data["cases"]) {

		const auto case_result = get_case_from_json(case_);

		cases.push_back(case_result);
	}

	const std::string name = path.filename().string();

	return ThirdPartyHpackTestCase{
		.description = description,
		.cases = cases,
		.name = name,
	};
}

[[nodiscard]] static std::vector<ThirdPartyHpackTestCase>
get_thirdparty_hpack_test_cases(const std::string& name) {

	const std::filesystem::path root_tests_dir =
	    std::filesystem::current_path() / ".." / "thirdparty" / "hpack-test-case";

	if(!std::filesystem::exists(root_tests_dir)) {
		throw std::runtime_error("Invalid test launch from invalid cwd!");
	}

	const std::filesystem::path dir = root_tests_dir / name;

	if(!std::filesystem::exists(dir)) {
		throw std::runtime_error(std::string{ "Invalid test dir name: " } + dir.string());
	}

	if(!std::filesystem::is_directory(dir)) {
		throw std::runtime_error(std::string{ "Invalid test dir name: (no a dir)" } + dir.string());
	}

	std::vector<ThirdPartyHpackTestCase> result{};

	for(auto const& dir_entry : std::filesystem::directory_iterator{ dir }) {

		if(!dir_entry.is_regular_file()) {
			continue;
		}

		const auto value = get_thirdparty_hpack_test_case(dir_entry.path());

		result.push_back(value);
	}

	return result;
}

[[nodiscard]] static std::unordered_map<std::string, std::string>
get_cpp_headers(const HttpHeaderFields& fields) {

	std::unordered_map<std::string, std::string> result{};

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, fields, i);

		result.insert_or_assign(std::string{ field.key }, std::string{ field.value });
	}

	return result;
}
