

#pragma once

#include <nlohmann/json.hpp>

#include <support/helpers/hpack.hpp>

namespace hpack {
[[nodiscard]] std::vector<tests::ThirdPartyHpackTestCase>
get_thirdparty_hpack_test_cases(const std::string& name);

}
