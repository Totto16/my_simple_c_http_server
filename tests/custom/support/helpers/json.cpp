

#include "./json.hpp"

#include "./cpp_types.hpp"

[[nodiscard]] static bool operator==(const JsonBoolean& json_boolean1,
                                     const JsonBoolean& json_boolean2) {
	return json_boolean1.value == json_boolean2.value;
}

[[nodiscard]] static bool operator==(const JsonNumber& json_number1,
                                     const JsonNumber& json_number2) {
	return json_number1.value == json_number2.value;
}

[[nodiscard]] static bool operator==(const JsonString* const json_string1,
                                     const JsonString* const json_string2) {
	return json_string_eq(json_string1, json_string2);
}

[[nodiscard]] bool operator==(const JsonVariant& json_variant1, const JsonVariant& json_variant2) {
	const auto tag1 = get_current_tag_type_for_json_variant(json_variant1);
	const auto tag2 = get_current_tag_type_for_json_variant(json_variant2);

	if(tag1 != tag2) {
		return false;
	}

	SWITCH_JSON_VARIANT(json_variant1) {
		CASE_JSON_VARIANT_IS_NULL() {

			IF_JSON_VARIANT_IS_NULL(json_variant2) {
				return true;
			}

			return false;
		}
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_BOOLEAN_CONST(json_variant1, boolean_1) {

			IF_JSON_VARIANT_IS_BOOLEAN_CONST(json_variant2, boolean_2) {

				return boolean_1 == boolean_2;
			}

			return false;
		}
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_NUMBER_CONST(json_variant1, number_1) {

			IF_JSON_VARIANT_IS_NUMBER_CONST(json_variant2, number_2) {

				return number_1 == number_2;
			}

			return false;
		}
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_STRING_CONST(json_variant1, string_1) {

			IF_JSON_VARIANT_IS_STRING_CONST(json_variant2, string_2) {

				return string_1 == string_2;
			}

			return false;
		}
		VARIANT_CASE_END();
		default: {
			return false;
		}
	}
}

std::ostream& operator<<(std::ostream& os, const JsonVariant& json_variant) {

	const auto str = json_variant_to_string_advanced(json_variant, { .indent_size = 2 });

	os << str;

	return os;
}
