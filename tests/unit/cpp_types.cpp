

#include "./cpp_types.hpp"

[[maybe_unused]] std::ostream&
operator<<(std::ostream& os, const std::unordered_map<std::string, std::string>& string_map) {
	os << "string map{\n";
	for(const auto& val : string_map) {
		os << val.first << ": " << val.second << "\n";
	}
	os << "}\n";

	return os;
}

doctest::String toString(const std::unordered_map<std::string, std::string>& string_map) {
	std::stringstream str{};
	str << string_map;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}
