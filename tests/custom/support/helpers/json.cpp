#include "./json.hpp"

#include "../generic.hpp"
#include "./cpp_types.hpp"

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

				return JsonStringCpp{ string_1 } == string_2;
			}

			return false;
		}
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_ARRAY_CONST(json_variant1, array_1) {

			IF_JSON_VARIANT_IS_ARRAY_CONST(json_variant2, array_2) {

				return JsonArrayCpp{ array_1.arr } == array_2.arr;
			}

			return false;
		}
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_OBJECT_CONST(json_variant1, object_1) {

			IF_JSON_VARIANT_IS_OBJECT_CONST(json_variant2, object_2) {

				return JsonObjectCpp{ object_1.obj } == object_2.obj;
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

[[nodiscard]] bool operator==(const JsonBoolean& json_boolean1, const JsonBoolean& json_boolean2) {
	return json_boolean1.value == json_boolean2.value;
}

[[nodiscard]] bool operator==(const JsonNumber& json_number1, const JsonNumber& json_number2) {
	return json_number1.value == json_number2.value;
}

JsonStringCpp::JsonStringCpp(JsonString* value) : m_value{ value } {}

[[nodiscard]] bool JsonStringCpp::operator==(const JsonStringCpp& json_string2) const {
	return json_string_eq(this->m_value, json_string2.m_value);
}

[[nodiscard]] bool JsonStringCpp::operator==(const JsonString* const json_string2) const {
	return json_string_eq(this->m_value, json_string2);
}

JsonArrayCpp::JsonArrayCpp(JsonArray* value) : m_value{ value } {}

[[nodiscard]] static bool json_array_eq_impl(const JsonArray* const json_array1,
                                             const JsonArray* const json_array2) {
	const size_t size1 = json_array_size(json_array1);
	const size_t size2 = json_array_size(json_array2);

	if(size1 != size2) {
		return false;
	}

	for(size_t i = 0; i < size1; ++i) {

		const auto val1 = json_array_at(json_array1, i);
		const auto val2 = json_array_at(json_array2, i);

		if(val1 != val2) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool JsonArrayCpp::operator==(const JsonArrayCpp& json_array2) const {
	return json_array_eq_impl(this->m_value, json_array2.m_value);
}

[[nodiscard]] bool JsonArrayCpp::operator==(const JsonArray* const json_array2) const {
	return json_array_eq_impl(this->m_value, json_array2);
}

JsonObjectCpp::JsonObjectCpp(JsonObject* value) : m_value{ value } {}

[[nodiscard]] static bool json_object_entry_value_eq(const JsonObjectEntry* const entry1,
                                                     const JsonObjectEntry* const entry2) {

	const auto value1 = json_object_entry_get_value(entry1);

	const auto value2 = json_object_entry_get_value(entry2);

	return value1 == value2;
}

[[nodiscard]] static bool json_object_eq_impl(const JsonObject* const json_object1,
                                              const JsonObject* const json_object2) {

	const size_t size1 = json_object_count(json_object1);
	const size_t size2 = json_object_count(json_object2);

	if(size1 != size2) {
		return false;
	}

	// as the iteration is not stable, we need to iterate over the one, and assert that the other
	// one has also the same entry!
	JsonObjectIter* iter1 = json_object_get_iterator(json_object1);
	CAutoFreePtr<JsonObjectIter> defer = { iter1, json_object_free_iterator };

	while(true) {

		const JsonObjectEntry* entry1 = json_object_iterator_next(iter1);

		if(entry1 == nullptr) {
			break;
		}

		const auto* const key1 = json_object_entry_get_key(entry1);

		assert(key1 != nullptr);

		const JsonObjectEntry* entry2 = json_object_get_entry_by_key(json_object2, key1);

		// the same key was not found
		if(entry2 == nullptr) {
			return false;
		}

		if(!json_object_entry_value_eq(entry1, entry2)) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool JsonObjectCpp::operator==(const JsonObjectCpp& json_object2) const {
	return json_object_eq_impl(this->m_value, json_object2.m_value);
}

[[nodiscard]] bool JsonObjectCpp::operator==(const JsonObject* const json_object2) const {
	return json_object_eq_impl(this->m_value, json_object2);
}

[[nodiscard]] JsonVariant JsonVariantCpp::null() {
	return new_json_variant_null();
}

[[nodiscard]] JsonVariant JsonVariantCpp::boolean(const bool& value) {
	return new_json_variant_boolean(JsonBoolean{ .value = value });
}

[[nodiscard]] JsonVariant JsonVariantCpp::number(const double& value) {
	return new_json_variant_number(JsonNumber{ .value = value });
}

[[nodiscard]] JsonVariant JsonVariantCpp::number(const int64_t& value) {
	return number(static_cast<double>(value));
}

[[nodiscard]] JsonVariant JsonVariantCpp::string(const std::string& value) {
	JsonString* const string = json_get_string_from_tstr_view(helpers::tstr_view_from_str(value));

	if(string == nullptr) {
		throw std::runtime_error("JSON string initialization failed");
	}

	return new_json_variant_string(string);
}

[[nodiscard]] JsonVariant JsonVariantCpp::array(std::initializer_list<JsonVariant>&& values) {
	JsonArray* const array = get_empty_json_array();

	if(array == nullptr) {
		throw std::runtime_error("JSON array initialization failed");
	}

	for(auto&& value : values) {
		const auto add_result = json_array_add_entry(array, value);
		if(!tstr_static_is_null(add_result)) {
			throw std::runtime_error(std::string{ "JSON array initialization failed:" } +
			                         string_from_tstr_static(add_result));
		}
	}

	return new_json_variant_array(array);
}
