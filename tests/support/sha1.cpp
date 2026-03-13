
#include "./sha1.hpp"

#include "./helpers/cpp_types.hpp"

[[nodiscard]] SizedBuffer details::Sha1BufferType::get_sized_buffer() const {
	if(is_error()) {
		assert(false && "can't get the buffer from a value with an error");
		return SizedBuffer{ .data = NULL, .size = 0 };
	}
	SizedBuffer sized_buffer = { .data = (void*)reinterpret_cast<const void*>(this->m_value.data()),
		                         .size = sha1_buffer_size };
	return sized_buffer;
}

[[nodiscard]] bool details::Sha1BufferType::operator==(const SizedBuffer& lhs) const {
	if(is_error()) {
		return false;
	}

	SizedBuffer rhs_sized_buffer = this->get_sized_buffer();

	return lhs == rhs_sized_buffer;
}

std::ostream& details::operator<<(std::ostream& os, const details::Sha1BufferType& buffer) {
	SizedBuffer sized_buffer = buffer.get_sized_buffer();
	os << sized_buffer;
	return os;
}
