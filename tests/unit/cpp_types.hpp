#pragma once

#include <doctest.h>

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

constexpr const size_t vector_max_for_printing_content = 40;

template <typename A, typename B>
[[maybe_unused]] std::ostream& operator<<(std::ostream& os, const std::pair<A, B>& pair) {
	os << "pair{" << pair.first << ", " << pair.second << "}";
	return os;
}

template <typename T>
[[maybe_unused]] std::ostream& operator<<(std::ostream& os, const std::vector<T>& vector) {
	if(vector.data() == NULL || vector.size() > vector_max_for_printing_content) {
		os << "vector{data=" << vector.data() << ", size=" << vector.size() << "}";
	} else {
		os << "vector{content={";
		for(size_t i = 0; i < vector.size(); ++i) {
			if(i != 0) {
				os << ", ";
			}
			os << vector.at(i);
		}
		os << "} }";
	}
	return os;
}

doctest::String toString(const std::unordered_map<std::string, std::string>& string_map);

[[maybe_unused]] std::ostream&
operator<<(std::ostream& os, const std::unordered_map<std::string, std::string>& string_map);

namespace doctest {
template <> struct StringMaker<std::unordered_map<std::string, std::string>> {
	static String convert(const std::unordered_map<std::string, std::string>& string_map) {
		return toString(string_map);
	}
};

template <typename T> struct StringMaker<std::vector<T>> {
	static String convert(const std::vector<T>& vec) { return toString(vec); }
};

template <typename A, typename B> struct StringMaker<std::pair<A, B>> {
	static String convert(const std::pair<A, B>& pair) { return toString(pair); }
};

} // namespace doctest
