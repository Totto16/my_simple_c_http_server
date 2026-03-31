

#include "./generic.hpp"

[[nodiscard]] ReadonlyBuffer helpers::buffer_from_string(const std::string& inp) {
	return ReadonlyBuffer{ .data = (const void*)inp.c_str(), .size = inp.size() };
}
[[nodiscard]] tstr helpers::tstr_from_utf8_string(const std::vector<std::uint8_t>& val) {
	const tstr buffer = tstr_from_len((const char*)val.data(), val.size());
	return buffer;
}

[[nodiscard]] std::vector<std::uint8_t> helpers::vector_from_string(const std::string& data) {
	std::vector<std::uint8_t> result = {};
	result.reserve(data.size());

	for(const auto& ch : data) {
		result.emplace_back(ch);
	}

	return result;
}

[[nodiscard]] ReadonlyBuffer helpers::buffer_from_raw_data(const std::vector<std::uint8_t>& data) {
	const ReadonlyBuffer buffer = { .data = (const void*)data.data(), .size = data.size() };
	return buffer;
}
