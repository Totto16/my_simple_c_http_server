

#pragma once

#include <json/json.h>

#include <ostream>

[[nodiscard]] bool operator==(const JsonVariant& json_variant1, const JsonVariant& json_variant2);

std::ostream& operator<<(std::ostream& os, const JsonVariant& json_variant);
