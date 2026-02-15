#pragma once

#include <doctest.h>

#include <utils/sized_buffer.h>

doctest::String toString(const SizedBuffer& buffer);

NODISCARD bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs);

std::ostream& operator<<(std::ostream& os, const SizedBuffer& buffer);
