

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

Errorable::Errorable() : m_value{ std::nullopt } {}

Errorable::Errorable(std::string&& value) : m_value{ std::move(value) } {}

[[nodiscard]] bool Errorable::is_error() const {
	return this->m_value.has_value();
}

[[nodiscard]] std::string Errorable::error() const {
	if(!this->m_value.has_value()) {
		throw std::runtime_error("can't get error, if there is none!");
	}
	return this->m_value.value();
}

IsNotError::IsNotError() {}

TstrStaticIsNull::TstrStaticIsNull() {}

[[nodiscard]] bool TstrStaticIsNull::operator==(const tstr_static& lhs) const {
	return tstr_static_is_null(lhs);
}

[[nodiscard]] bool operator==(const tstr_static& lhs, const TstrStaticIsNull& rhs) {
	return rhs == lhs;
}

[[nodiscard]] std::string string_from_tstr(const tstr& value) {
	if(tstr_is_null(&value)) {
		throw std::runtime_error("tstr is NULL!");
	}

	return std::string{ tstr_cstr(&value), tstr_len(&value) };
}

[[nodiscard]] std::string string_from_tstr_static(const tstr_static& value) {
	if(tstr_static_is_null(value)) {
		throw std::runtime_error("tstr_static is NULL!");
	}

	return std::string{ value.ptr, value.len };
}

[[nodiscard]] tstr tstr_from_string(const std::string& value) {
	return tstr_from_len(value.c_str(), value.size());
}

std::ostream& operator<<(std::ostream& os, const IsNotError& /* error */) {
	os << "{}";
	return os;
}

std::ostream& operator<<(std::ostream& os, const TstrStaticIsNull& /* is_null */) {
	os << "{}";
	return os;
}

[[nodiscard]] std::vector<std::uint8_t> helpers::raw_data_from_buffer(const SizedBuffer& buffer) {

	std::vector<std::uint8_t> result{};
	result.reserve(buffer.size);

	for(size_t i = 0; i < buffer.size; ++i) {
		result.push_back(((uint8_t*)buffer.data)[i]);
	}

	return result;
}

size_t g_doctest_timeout_multiplier = 1;

static std::string_view sv_from_tstr(const tstr& str) {
	return { tstr_cstr(&str), tstr_len(&str) };
}

static std::string_view sv_from_tstr_static(const tstr_static& str) {
	return { str.ptr, str.len };
}

std::ostream& operator<<(std::ostream& os, const tstr& str) {
	if(tstr_is_null(&str)) {
		os << "<tstr NULL>";
		return os;
	}

	os << sv_from_tstr(str);
	return os;
}

std::ostream& operator<<(std::ostream& os, const tstr_static& str) {
	if(tstr_static_is_null(str)) {
		os << "<tstr_static NULL>";
		return os;
	}

	os << sv_from_tstr_static(str);
	return os;
}
