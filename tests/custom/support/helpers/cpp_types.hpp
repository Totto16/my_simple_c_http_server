#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tstr.h>
#include <vector>

#include "./c_types.hpp"

namespace details {
constexpr const size_t vector_max_for_printing_content = 40;
} // namespace details

template <typename A, typename B>
std::ostream& operator<<(std::ostream& os, const std::pair<A, B>& pair) {
	os << "pair{" << pair.first << ", " << pair.second << "}";
	return os;
}

template <typename T> std::ostream& print_vector(std::ostream& os, const std::vector<T>& vector) {
	if(vector.data() == NULL || vector.size() > details::vector_max_for_printing_content) {
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
		os << "} , size=" << vector.size() << "}";
	}
	return os;
}

template <typename T> std::ostream& operator<<(std::ostream& os, const std::vector<T>& vector) {
	return print_vector(os, vector);
}

template <typename T> std::ostream& operator<<(std::ostream& os, const std::optional<T>& val) {
	if(!val.has_value()) {
		os << "std::nullopt";
		return os;
	}

	os << "std::optional{ " << val.value() << " }";

	return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<std::pair<std::string, std::string>>& string_map);

template <typename T> using CAutoFreePtr = std::unique_ptr<T, void (*)(T*)>;

[[nodiscard]] std::string string_from_tstr(const tstr& value);

[[nodiscard]] std::string string_from_tstr_static(const tstr_static& value);

[[nodiscard]] tstr tstr_from_string(const std::string& value);

template <typename T>
[[nodiscard]] static inline bool vec_contains(const std::vector<T>& vec, const T& val) {
	return std::find(vec.cbegin(), vec.cend(), val) != vec.cend();
}
template <typename T>
[[nodiscard]] static inline bool vec_contains_duplicate(const std::vector<T>& vec) {

	for(auto it = vec.cbegin(); it != vec.cend(); ++it) {
		const auto start_search_it = it + 1;
		if(start_search_it == vec.cend()) {
			break;
		}

		const auto res = std::find(start_search_it, vec.cend(), *it);

		if(res != vec.cend()) {
			return true;
		}
	}

	return false;
}

// to have all generated error variant types

#include <http/hpack.h>
#include <http/hpack_huffman.h>
#include <http/v2.h>
#include <utils/utf8_helper.h>

// END

#include <optional>

CPP_DEFINE_ERROR_VARIANTS()

template <typename T>
concept IsCErrorVariant = cpp::error_variants::IsErrorVariant<T>::value;

struct Errorable {
  private:
	std::optional<std::string> m_value;

	explicit Errorable();

	explicit Errorable(std::string&& value);

  public:
	template <typename T>
	    requires(IsCErrorVariant<T>)
	static Errorable from_c_variant_with_error(const T& value) {
		const std::optional<tstr_static> temp =
		    cpp::error_variants::ErrorVariantConversionImpl<T>::to_cpp_type(value);
		if(temp.has_value()) {
			return Errorable{ string_from_tstr_static(temp.value()) };
		}

		return Errorable{};
	}

	[[nodiscard]] bool is_error() const;

	[[nodiscard]] std::string error() const;
};

struct IsNotError {
  public:
	IsNotError();

	friend std::ostream& operator<<(std::ostream& os, const IsNotError& error);

	template <typename T>
	    requires(IsCErrorVariant<T>)
	[[nodiscard]] bool operator==(const T& lhs) const {
		return !Errorable::from_c_variant_with_error<T>(lhs).is_error();
	}
};

struct TstrStaticIsNull {
  public:
	TstrStaticIsNull();

	friend std::ostream& operator<<(std::ostream& os, const TstrStaticIsNull& error);

	[[nodiscard]] bool operator==(const tstr_static& lhs) const;
};

[[nodiscard]] bool operator==(const tstr_static& lhs, const TstrStaticIsNull& rhs);

std::ostream& operator<<(std::ostream& os, const IsNotError& error);

#define REQUIRE_IS_NOT_ERROR(val) REQUIRE_EQ(IsNotError{}, val)

#define REQUIRE_IS_ERROR(val) REQUIRE_NE(IsNotError{}, val)

template <typename T>
    requires(IsCErrorVariant<T>)
static std::ostream& operator<<(std::ostream& os, const T& entry) {
	const auto& value = Errorable::from_c_variant_with_error<T>(entry);

	if(!value.is_error()) {
		os << "Printing not supported for non error variant of:" << typeid(entry).name();
		return os;
	}

	os << typeid(entry).name() << "{ is_error: true, error: " << value.error() << " }";

	return os;
}

namespace helpers {

[[nodiscard]] std::vector<std::uint8_t> raw_data_from_buffer(const SizedBuffer& buffer);
}

extern size_t g_doctest_timeout_multiplier;

std::ostream& operator<<(std::ostream& os, const tstr& str);

std::ostream& operator<<(std::ostream& os, const tstr_static& str);
