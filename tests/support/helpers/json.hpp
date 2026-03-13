

#pragma once

#include <nlohmann/json.hpp>

#include "./hpack.hpp"

namespace hpack {
[[nodiscard]] std::vector<tests::ThirdPartyHpackTestCase>
get_thirdparty_hpack_test_cases(const std::string& name);

}
