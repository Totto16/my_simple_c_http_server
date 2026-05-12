
#include "./sha1.hpp"

#include "./helpers/cpp_types.hpp"

[[nodiscard]] ReadonlyBuffer details::Sha1BufferType::get_buffer() const {
	if(is_error()) {
		assert(false && "can't get the buffer from a value with an error");
		return ReadonlyBuffer{ .data = NULL, .size = 0 };
	}
	ReadonlyBuffer buffer = { .data = reinterpret_cast<GenericDataConst>(this->m_value.data()),
		                      .size = sha1_buffer_size };
	return buffer;
}

[[nodiscard]] bool details::Sha1BufferType::operator==(const SizedBuffer& lhs) const {
	if(is_error()) {
		return false;
	}

	ReadonlyBuffer rhs_buffer = this->get_buffer();

	return lhs == rhs_buffer;
}

std::ostream& details::operator<<(std::ostream& os, const details::Sha1BufferType& buffer) {
	ReadonlyBuffer ro_buffer = buffer.get_buffer();
	os << ro_buffer;
	return os;
}
