

#include "./cpp_types.hpp"

std::ostream& operator<<(std::ostream& os,
                         const std::vector<std::pair<std::string, std::string>>& string_map) {
	os << "string map{\n";
	for(const auto& val : string_map) {
		os << val.first << ": " << val.second << "\n";
	}
	os << "}\n";

	return os;
}

IsNotError::IsNotError() {}

[[nodiscard]] std::string string_from_tstr(const tstr& value) {
	if(tstr_is_null(&value)) {
		throw std::runtime_error("tstr is NULL!");
	}

	return std::string{ tstr_cstr(&value), tstr_len(&value) };
}

[[nodiscard]] tstr tstr_from_string(const std::string& value) {
	return tstr_from_len(value.c_str(), value.size());
}

std::ostream& operator<<(std::ostream& os, const IsNotError& /* error */) {
	os << "{}";
	return os;
}

[[nodiscard]] std::vector<std::uint8_t> helpers::raw_data_from_buffer(const SizedBuffer& buffer) {

	std::vector<std::uint8_t> result{};
	result.reserve(buffer.size);

	for(size_t i = 0; i < buffer.size; ++i) {
		result.push_back(((uint8_t*)buffer.data)[i]);
	}

	return result;
}

size_t g_doctest_timeout_multiplier = 1;
