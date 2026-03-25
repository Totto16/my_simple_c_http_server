

#include "./number_parsing.h"

NODISCARD int64_t parse_i64(const tstr_view to_parse, OUT_PARAM(bool) const success) {

	if(to_parse.len == 0) {
		*success = false;
		return 0;
	}

	bool has_sign = false;
	size_t i = 0;

	if(to_parse.data[0] == '-') {
		has_sign = true;
		i++;
	} else if(to_parse.data[0] == '+') {
		i++;
	}

	if(i == to_parse.len) {
		*success = false;
		return 0;
	}

	uint64_t result_temp = 0;
	for(; i < to_parse.len; i++) {
		if(to_parse.data[i] < '0' || to_parse.data[i] > '9') {
			*success = false;
			return 0;
		}
		const uint64_t old_result = result_temp;
		result_temp =
		    (result_temp * 10UL) + (to_parse.data[i] - '0'); // NOLINT(readability-magic-numbers)

		// check for overflow
		if(result_temp < old_result) {
			*success = false;
			return 0;
		}
	}

	// check for too big values
	if(has_sign) {

		// too big, when we invert it
		if(result_temp > (((uint64_t)INT64_MAX) + 1)) {
			*success = false;
			return 0;
		}

		const int64_t result = -((int64_t)result_temp);

		*success = true;
		return result;
	}

	if(result_temp > INT64_MAX) {
		*success = false;
		return 0;
	}

	*success = true;
	return (int64_t)result_temp;
}

NODISCARD uint64_t parse_u64(tstr_view to_parse, OUT_PARAM(bool) success) {
	if(to_parse.len == 0) {
		*success = false;
		return 0;
	}

	uint64_t result = 0;
	for(size_t i = 0; i < to_parse.len; i++) {
		if(to_parse.data[i] < '0' || to_parse.data[i] > '9') {
			*success = false;
			return 0;
		}
		const uint64_t old_result = result;
		result = (result * 10UL) + (to_parse.data[i] - '0'); // NOLINT(readability-magic-numbers)

		// check for overflow
		if(result < old_result) {
			*success = false;
			return 0;
		}
	}

	*success = true;
	return result;
}
