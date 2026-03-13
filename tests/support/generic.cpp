

#include "./generic.hpp"

[[nodiscard]] SizedBuffer buffer_from_string(const std::string& inp) {
	return { .data = (void*)inp.c_str(), .size = inp.size() };
}
