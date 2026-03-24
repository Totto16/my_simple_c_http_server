

#pragma once

#include "utils.h"

#include "tstr.h"

NODISCARD int64_t parse_i64(tstr_view to_parse, OUT_PARAM(bool) success);

NODISCARD uint64_t parse_u64(tstr_view to_parse, OUT_PARAM(bool) success);
