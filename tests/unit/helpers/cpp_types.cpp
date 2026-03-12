

#include "./cpp_types.hpp"

[[maybe_unused]] std::ostream&
operator<<(std::ostream& os, const std::vector<std::pair<std::string, std::string>>& string_map) {
	os << "string map{\n";
	for(const auto& val : string_map) {
		os << val.first << ": " << val.second << "\n";
	}
	os << "}\n";

	return os;
}

IsNotError::IsNotError() {}
