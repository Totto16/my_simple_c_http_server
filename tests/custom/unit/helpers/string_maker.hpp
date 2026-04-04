

#pragma once

#include <doctest.h>

#include <sstream>
#include <vector>

#include <support/helpers.hpp>
#include <support/helpers/hpack.hpp>
#include <support/helpers/http.hpp>

template <typename T>
concept is_cpp_stream_printable = requires(T val, std::ostream& os) {
	{ os << val } -> std::convertible_to<std::ostream&>;
};

template <typename T>
doctest::String os_stream_formattable_to_doctest(const T& value)
    requires(is_cpp_stream_printable<T>)
{
	std::stringstream str{};
	str << value;
	std::string string = str.str();
	return doctest::String{ string.c_str(),
		                    static_cast<doctest::String::size_type>(string.size()) };
}

#include <support/helpers.hpp>

namespace doctest {
template <> struct StringMaker<std::vector<std::pair<std::string, std::string>>> {
	static String convert(const std::vector<std::pair<std::string, std::string>>& string_map) {
		return ::os_stream_formattable_to_doctest(string_map);
	}
};

template <typename T> struct StringMaker<std::optional<T>> {
	static String convert(const std::optional<T>& val) {
		return ::os_stream_formattable_to_doctest(val);
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

template <> struct StringMaker<IsNotError> {
	static String convert(const IsNotError& error) {
		return ::os_stream_formattable_to_doctest(error);
	}
};

template <> struct StringMaker<TstrStaticIsNull> {
	static String convert(const TstrStaticIsNull& is_null) {
		return ::os_stream_formattable_to_doctest(is_null);
	}
};

template <typename T>
    requires(IsCErrorVariant<T>)
struct StringMaker<T> {
	static String convert(const T& val) { return ::os_stream_formattable_to_doctest(val); }
};

template <typename T> struct StringMaker<helpers::OptionalOr<T>> {
	static String convert(const helpers::OptionalOr<T>& val) {
		return ::os_stream_formattable_to_doctest(val);
	}
};

template <> struct StringMaker<CompressionEntry> {
	static String convert(const CompressionEntry& entry) {
		return ::os_stream_formattable_to_doctest(entry);
	}
};

template <> struct StringMaker<test::DynamicTable> {
	static String convert(const test::DynamicTable& table) {
		return ::os_stream_formattable_to_doctest(table);
	}
};

template <> struct StringMaker<hpack::DynamicTableC> {
	static String convert(const hpack::DynamicTableC& table) {
		return ::os_stream_formattable_to_doctest(table);
	}
};

template <> struct StringMaker<hpack::DynamicTableCpp> {
	static String convert(const hpack::DynamicTableCpp& table) {
		return ::os_stream_formattable_to_doctest(table);
	}
};

template <> struct StringMaker<hpack::DynamicEntry> {
	static String convert(const hpack::DynamicEntry& entry) {
		return ::os_stream_formattable_to_doctest(entry);
	}
};

template <> struct StringMaker<tstr> {
	static String convert(const tstr& str) { return ::os_stream_formattable_to_doctest(str); }
};

template <> struct StringMaker<tstr_static> {
	static String convert(const tstr_static& str) {
		return ::os_stream_formattable_to_doctest(str);
	}
};

template <> struct StringMaker<JsonVariant> {
	static String convert(const JsonVariant& json_variant) {
		return ::os_stream_formattable_to_doctest(json_variant);
	}
};

} // namespace doctest
