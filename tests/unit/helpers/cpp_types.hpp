#pragma once

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <tstr.h>

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

std::ostream& operator<<(std::ostream& os,
                         const std::unordered_map<std::string, std::string>& string_map);

template <typename T> doctest::String os_stream_formattable_to_doctest(const T& value) {
	std::stringstream str{};
	str << value;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

namespace doctest {
template <> struct StringMaker<std::unordered_map<std::string, std::string>> {
	static String convert(const std::unordered_map<std::string, std::string>& string_map) {
		return ::os_stream_formattable_to_doctest(string_map);
	}
};

template <typename T> struct StringMaker<std::vector<T>> {
	static String convert(const std::vector<T>& vec) {
		return ::os_stream_formattable_to_doctest(vec);
	}
};

template <typename A, typename B> struct StringMaker<std::pair<A, B>> {
	static String convert(const std::pair<A, B>& pair) {
		return ::os_stream_formattable_to_doctest(pair);
	}
};

} // namespace doctest

template <typename T> struct CppDefer {
  public:
	using FreeFn = std::function<void(T*)>;

  private:
	T* m_state;
	FreeFn m_free_fn;

  public:
	CppDefer(T* state, const FreeFn& free_fn) : m_state{ state }, m_free_fn{ free_fn } {}

	CppDefer(CppDefer&&) = delete;

	CppDefer(const CppDefer&) = delete;

	CppDefer& operator=(const CppDefer&) = delete;

	CppDefer operator=(CppDefer&&) = delete;

	~CppDefer() { this->m_free_fn(this->m_state); }
};

[[nodiscard]] static inline std::string string_from_tstr(const tstr value) {
	return std::string{ tstr_cstr(&value), tstr_len(&value) };
}
