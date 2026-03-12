#pragma once

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <tstr.h>
#include <vector>

#include "./c_types.hpp"

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

			const auto& val = vector.at(i);

			if constexpr(std::is_same_v<T, char> || std::is_same_v<T, std::uint8_t>) {
				if(isprint(val)) {
					os << val;
				} else {
					os << get_hex_value_for_u8(val);
				}
			} else {
				os << val;
			}
		}
		os << "} }";
	}
	return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<std::pair<std::string, std::string>>& string_map);

template <typename T> doctest::String os_stream_formattable_to_doctest(const T& value) {
	std::stringstream str{};
	str << value;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

namespace doctest {
template <> struct StringMaker<std::vector<std::pair<std::string, std::string>>> {
	static String convert(const std::vector<std::pair<std::string, std::string>>& string_map) {
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

	T const* get() const { return this->m_state; }

	~CppDefer() { this->m_free_fn(this->m_state); }
};

[[nodiscard]] static inline std::string string_from_tstr(const tstr& value) {
	return std::string{ tstr_cstr(&value), tstr_len(&value) };
}

[[nodiscard]] static inline tstr tstr_from_string(const std::string& value) {
	return tstr_from_len(value.c_str(), value.size());
}

[[nodiscard]] static inline tstr operator""_tstr(const char* str, std::size_t len) {
	return tstr_from_static_cstr_with_len(str, len);
}

[[nodiscard]] static inline SizedBuffer
buffer_from_raw_data(const std::vector<std::uint8_t>& data) {
	const SizedBuffer buffer = { .data = (void*)data.data(), .size = data.size() };
	return buffer;
}

template <typename T>
[[nodiscard]] static inline bool vec_contains(const std::vector<T>& vec, const T& val) {
	return std::find(vec.begin(), vec.end(), val) != vec.end();
}

template <typename T>
concept is_errorable_type = requires(T) {
	{ T::is_error } -> std::convertible_to<bool>;
};

struct IsNotError {
  public:
	IsNotError();

	inline friend std::ostream& operator<<(std::ostream& os, const IsNotError& /* error */) {
		os << "{}";
		return os;
	}

	template <typename T>
	    requires(is_errorable_type<T>)
	[[nodiscard]] bool operator==(const T& lhs) const {
		return !lhs.is_error;
	}
};

#define REQUIRE_IS_NOT_ERROR(val) REQUIRE_EQ(IsNotError{}, val)

namespace doctest {
template <> struct StringMaker<IsNotError> {
	static String convert(const IsNotError& error) {
		return ::os_stream_formattable_to_doctest(error);
	}
};

} // namespace doctest

template <typename T>
    requires(is_errorable_type<T> &&
             requires(T val) {
	             { val.data.error } -> std::convertible_to<const char*>;
             })
static std::string get_error_from(const T& entry) {
	return std::string{ entry.data.error };
}

template <typename T>
    requires(is_errorable_type<T>)
static std::ostream& operator<<(std::ostream& os, const T& entry) {
	if(!entry.is_error) {
		os << "Printing not supported for non error variant of:" << typeid(entry).name();
		return os;
	}

	os << typeid(entry).name() << "{ is_error: true, error:" << get_error_from<T>(entry) << " }";

	return os;
}

namespace doctest {
template <typename T>
    requires(is_errorable_type<T>)
struct StringMaker<T> {
	static String convert(const T& val) { return ::os_stream_formattable_to_doctest(val); }
};
} // namespace doctest
