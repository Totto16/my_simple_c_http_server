
#pragma once

#include "utils/sized_buffer.h"

#include <string>

[[nodiscard]] SizedBuffer buffer_from_string(const std::string& inp);
