
#include "./json.h"

#include <tmap.h>
#include <tvec.h>

// see: https://datatracker.ietf.org/doc/html/rfc8259

TVEC_DEFINE_VEC_TYPE(JsonVariant)

typedef TVEC_TYPENAME(JsonVariant) JsonVariantArr;

struct JsonArrayImpl {
	JsonVariantArr value;
};

// keys can only be strings
typedef struct {
	JsonString string;
} JsonObjectKey;

TMAP_DEFINE_MAP_TYPE(JsonObjectKey, JsonObjectKeyName, JsonVariant, JsonVariantMapImpl)

typedef TMAP_TYPENAME_MAP(JsonVariantMapImpl) JsonVariantMap;

struct JsonObjectImpl {
	JsonVariantMap value;
};

NODISCARD JsonParseResult json_variant_parse_from_str(const tstr_view str) {
	UNUSED(str);
	return new_json_parse_result_error(TSTR_STATIC_LIT("TODO"));
}

NODISCARD JsonParseResult json_variant_parse_from_file(const tstr_view str) {
	UNUSED(str);
	return new_json_parse_result_error(TSTR_STATIC_LIT("TODO"));
}

static void free_json_object(JsonObject* const json_obj) { // NOLINT(misc-no-recursion)
	TMAP_TYPENAME_ITER(JsonVariantMapImpl)
	iter = TMAP_ITER_INIT(JsonVariantMapImpl, &(json_obj->value));

	TMAP_TYPENAME_ENTRY(JsonVariantMapImpl) value;

	while(TMAP_ITER_NEXT(JsonVariantMapImpl, &iter, &value)) {

		free_json_variant(&value.value);
	}

	TMAP_FREE(JsonVariantMapImpl, &(json_obj->value));
}

static void free_json_array(JsonArray* const json_arr) { // NOLINT(misc-no-recursion)
	for(size_t i = 0; i < TVEC_LENGTH(JsonVariant, json_arr->value); ++i) {
		JsonVariant* const value = TVEC_GET_AT_MUT(JsonVariant, &(json_arr->value), i);
		free_json_variant(value);
	}
	TVEC_FREE(JsonVariant, &(json_arr->value));
}

static void free_json_string(const JsonString json_string) {
	JsonString temp = json_string;
	tstr_free(&(temp.value));
}

void free_json_variant(JsonVariant* const json_variant) {
	SWITCH_JSON_VARIANT(*json_variant) {
		CASE_JSON_VARIANT_IS_OBJECT_CONST(*json_variant) {
			free_json_object(object.obj);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_ARRAY_CONST(*json_variant) {
			free_json_array(array.arr);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_NUMBER_IGN() {}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_STRING_CONST(*json_variant) {
			free_json_string(string);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_BOOLEAN_IGN() {}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VARIANT_IS_NULL() {}
		break;
		VARIANT_CASE_END();
		default: {
			break;
		}
	}

	*json_variant = new_json_variant_null();
}

NODISCARD tstr json_variant_to_string(const JsonVariant json_variant) {
	return json_variant_to_string_advanced(json_variant,
	                                       (JsonSerializeOptions){ .indent_size = 0 });
}

NODISCARD tstr json_variant_to_string_advanced(JsonVariant json_variant,
                                               JsonSerializeOptions options) {

	tstr result = tstr_null();

	UNUSED(json_variant);
	UNUSED(options);

	return result;
}
