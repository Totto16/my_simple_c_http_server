


#include "./c_types.hpp"


doctest::String toString(const SizedBuffer& buffer) {
	std::stringstream str{};
	str << buffer;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}


NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs) {

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
