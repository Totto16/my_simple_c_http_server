

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

[[nodiscard]] SizedBuffer buffer_from_raw_data(const std::vector<std::uint8_t>& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

std::ostream& operator<<(std::ostream& os, const IsNotError& /* error */) {
	os << "{}";
	return os;
}
