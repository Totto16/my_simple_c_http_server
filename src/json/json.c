
#include "./json.h"

#include <utils/path.h>
#include <utils/sized_buffer.h>

#include <tmap.h>
#include <tvec.h>

// see: https://datatracker.ietf.org/doc/html/rfc8259

TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(JsonVariant)

typedef TVEC_TYPENAME(JsonVariant) JsonVariantArr;

struct JsonArrayImpl {
	JsonVariantArr value;
};

// keys can only be strings
typedef struct {
	JsonString string;
} JsonObjectKey;

TMAP_DEFINE_AND_IMPLEMENT_MAP_TYPE(JsonObjectKey, JsonObjectKeyName, JsonVariant,
                                   JsonVariantMapImpl)

TMAP_HASH_FUNC_SIG(JsonObjectKey, JsonObjectKeyName) {
	return TMAP_HASH_BYTES(tstr_cstr(&key.string.value), tstr_len(&key.string.value));
}

TMAP_EQ_FUNC_SIG(JsonObjectKey, JsonObjectKeyName) {
	return tstr_eq(&key1.string.value, &key2.string.value);
}

typedef TMAP_TYPENAME_MAP(JsonVariantMapImpl) JsonVariantMap;

struct JsonObjectImpl {
	JsonVariantMap value;
};

static void tstr_view_advance(tstr_view* const str, size_t amount) {
	assert(str->len >= amount);
	str->data += amount;
	str->len -= amount;
}

static bool json_parse_impl_is_ws(const char value) {
	/*        ws = *(
	      %x20 /              ; Space
	      %x09 /              ; Horizontal tab
	      %x0A /              ; Line feed or New line
	      %x0D )              ; Carriage return */

	switch(value) {
		case ' ':
		case '\t':
		case '\n':
		case '\r': {
			return true;
		}
		default: {
			return false;
		}
	}
}

static void json_parse_impl_skip_ws(tstr_view* const str) {

	size_t offset = 0;

	while(str->len > offset) {

		const char value = str->data[offset];

		if(json_parse_impl_is_ws(value)) {
			++offset;
			continue;
		}

		break;
	}

	if(offset == 0) {
		return;
	}

	tstr_view_advance(str, offset);
}

NODISCARD static JsonParseResult json_parse_impl_parse_boolean(tstr_view* const str) {

	// boolean = false / true
	// false = %x66.61.6c.73.65   ; false
	// true  = %x74.72.75.65      ; true

	{
		const tstr_static false_value = TSTR_STATIC_LIT("false");
		if(tstr_view_starts_with(*str, false_value.ptr)) {

			tstr_view_advance(str, false_value.len);

			return new_json_parse_result_ok(
			    new_json_variant_boolean((JsonBoolean){ .value = false }));
		}
	}

	{
		const tstr_static true_value = TSTR_STATIC_LIT("true");
		if(tstr_view_starts_with(*str, true_value.ptr)) {

			tstr_view_advance(str, true_value.len);

			return new_json_parse_result_ok(
			    new_json_variant_boolean((JsonBoolean){ .value = true }));
		}
	}

	return new_json_parse_result_error(TSTR_STATIC_LIT("not a boolean"));
}

NODISCARD static JsonParseResult json_parse_impl_parse_null(tstr_view* const str) {

	//  null  = %x6e.75.6c.6c      ; null

	{
		const tstr_static null_value = TSTR_STATIC_LIT("null");
		if(tstr_view_starts_with(*str, null_value.ptr)) {

			tstr_view_advance(str, null_value.len);

			return new_json_parse_result_ok(new_json_variant_null());
		}
	}

	return new_json_parse_result_error(TSTR_STATIC_LIT("not null"));
}

NODISCARD static JsonObject* get_empty_json_object_impl(void) {
	JsonObject* const object = malloc(sizeof(JsonObject));

	if(object == NULL) {
		return NULL;
	}

	object->value = TMAP_EMPTY(JsonVariantMapImpl);
	return object;
}

NODISCARD static tstr_static json_object_add_entry_impl(JsonObject* const json_object,
                                                        const JsonObjectKey key,
                                                        const JsonVariant variant) {

	const TmapInsertResult result =
	    TMAP_INSERT(JsonVariantMapImpl, &(json_object->value), key, variant, false);

	switch(result) {
		case TmapInsertResultOk: {
			return tstr_static_null();
		}
		case TmapInsertResultErr: {
			return TSTR_STATIC_LIT("json object add error");
		}
		case TmapInsertResultWouldOverwrite: {
			return TSTR_STATIC_LIT("duplicate key detected");
		}
		default: {
			return TSTR_STATIC_LIT("json object add unknown error");
		}
	}
}

NODISCARD static JsonParseResult json_parse_impl_parse_string(tstr_view* str);

NODISCARD static JsonParseResult json_parse_impl_parse_value(tstr_view* str);

static void free_json_string(JsonString json_string);

NODISCARD static tstr_static json_parse_impl_parse_object_member(tstr_view* const str,
                                                                 JsonObject* const json_object) {
	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	// member = string name-separator value

	if(str->len == 0) {
		return TSTR_STATIC_LIT("empty object member");
	}

	const JsonParseResult string_result = json_parse_impl_parse_string(str);

	IF_JSON_PARSE_RESULT_IS_ERROR_CONST(string_result) {
		return error.error;
	}

	const JsonVariant key_raw = json_parse_result_get_as_ok(string_result);

	IF_JSON_VARIANT_IS_NOT_STRING(key_raw) {
		return TSTR_STATIC_LIT("implementation error: string parser didn't return a string");
	}

	const JsonString key = json_variant_get_as_string(key_raw);

#define FREE_AT_END() \
	do { \
		free_json_string(key); \
	} while(false)

	{
		// name-separator  = ws %x3A ws  ; : colon

		json_parse_impl_skip_ws(str);

		if(str->len == 0) {
			FREE_AT_END();
			return TSTR_STATIC_LIT("empty object member");
		}

		if(str->data[0] != ':') {
			FREE_AT_END();
			return TSTR_STATIC_LIT("wrong  name-separator");
		}

		json_parse_impl_skip_ws(str);
	}

	const JsonParseResult value_result = json_parse_impl_parse_value(str);

	IF_JSON_PARSE_RESULT_IS_ERROR_CONST(value_result) {
		FREE_AT_END();
		return error.error;
	}

	JsonVariant value = json_parse_result_get_as_ok(value_result);

#undef FREE_AT_END
#define FREE_AT_END() \
	do { \
		free_json_string(key); \
		free_json_variant(&value); \
	} while(false)

	const tstr_static add_result =
	    json_object_add_entry_impl(json_object, (JsonObjectKey){ .string = key }, value);

	if(!tstr_static_is_null(add_result)) {
		FREE_AT_END();
		return add_result;
	}

	return tstr_static_null();
}

#undef FREE_AT_END

static void free_json_object(JsonObject* const json_obj);

NODISCARD static JsonParseResult json_parse_impl_parse_object(tstr_view* const str) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2

	//       object = begin-object [ member *( value-separator member ) ] end-object
	// member = string name-separator value
	// begin-object    = ws %x7B ws  ; { left curly bracket
	// end-object      = ws %x7D ws  ; } right curly bracket
	// name-separator  = ws %x3A ws  ; : colon
	// value-separator = ws %x2C ws  ; , comma

	if(str->len == 0) {
		return new_json_parse_result_error(TSTR_STATIC_LIT("empty object"));
	}

	{ // begin-object

		json_parse_impl_skip_ws(str);

		if(str->len == 0) {
			return new_json_parse_result_error(TSTR_STATIC_LIT("empty object"));
		}

		if(str->data[0] != '{') {
			return new_json_parse_result_error(TSTR_STATIC_LIT("wrong begin-object"));
		}

		tstr_view_advance(str, 1);

		if(str->len == 0) {
			return new_json_parse_result_error(TSTR_STATIC_LIT("empty object"));
		}

		json_parse_impl_skip_ws(str);

		if(str->len == 0) {
			return new_json_parse_result_error(TSTR_STATIC_LIT("empty object"));
		}
	}

	// either members or a object_end

	json_parse_impl_skip_ws(str);

	if(str->len == 0) {
		return new_json_parse_result_error(TSTR_STATIC_LIT("empty object"));
	}

	const char next_char = str->data[0];

	if(next_char == '}') {
		// end-object

		tstr_view_advance(str, 1);

		json_parse_impl_skip_ws(str);

		JsonObject* const object = get_empty_json_object_impl();

		if(object == NULL) {
			return new_json_parse_result_error(TSTR_STATIC_LIT("OOM"));
		}

		return new_json_parse_result_ok(new_json_variant_object(object));
	}

	JsonObject* const object = get_empty_json_object_impl();

	if(object == NULL) {
		return new_json_parse_result_error(TSTR_STATIC_LIT("OOM"));
	}

#define FREE_AT_END() \
	do { \
		free_json_object(object); \
	} while(false)

	{
		// member *( value-separator member )

		tstr_static member_result = json_parse_impl_parse_object_member(str, object);

		if(!tstr_static_is_null(member_result)) {
			FREE_AT_END();
			return new_json_parse_result_error(member_result);
		}

		while(true) {
			// end-object or value-separator

			json_parse_impl_skip_ws(str);

			if(str->len == 0) {
				return new_json_parse_result_error(TSTR_STATIC_LIT("empty object"));
			}

			const char end_char = str->data[0];

			if(end_char == '}') {
				// end-object

				tstr_view_advance(str, 1);

				json_parse_impl_skip_ws(str);

				return new_json_parse_result_ok(new_json_variant_object(object));
			}

			if(end_char != ',') {
				FREE_AT_END();
				return new_json_parse_result_error(
				    TSTR_STATIC_LIT("invalid continuation of member in object"));
			}

			{
				// value-separator

				tstr_view_advance(str, 1);

				json_parse_impl_skip_ws(str);
			}

			tstr_static member_result_2 = json_parse_impl_parse_object_member(str, object);

			if(!tstr_static_is_null(member_result_2)) {
				FREE_AT_END();
				return new_json_parse_result_error(member_result_2);
			}
		}
	}
}

#undef FREE_AT_END

NODISCARD static JsonParseResult json_parse_impl_parse_array(tstr_view* const str) {
	UNUSED(str);
	return new_json_parse_result_error(TSTR_STATIC_LIT("TODO"));
}

NODISCARD static JsonParseResult json_parse_impl_parse_number(tstr_view* const str) {
	UNUSED(str);
	return new_json_parse_result_error(TSTR_STATIC_LIT("TODO"));
}

NODISCARD static JsonParseResult json_parse_impl_parse_string(tstr_view* const str) {
	UNUSED(str);
	return new_json_parse_result_error(TSTR_STATIC_LIT("TODO"));
}

NODISCARD static JsonParseResult json_parse_impl_parse_value(tstr_view* const str) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	//       value = false / null / true / object / array / number / string

	if(str->len == 0) {
		return new_json_parse_result_error(TSTR_STATIC_LIT("empty value"));
	}

	const char first_char = str->data[0];
	switch(first_char) {
		case 'f': {
			return json_parse_impl_parse_boolean(str);
		}
		case 'n': {
			return json_parse_impl_parse_null(str);
		}
		case 't': {
			return json_parse_impl_parse_boolean(str);
		}
		case '{': {
			return json_parse_impl_parse_object(str);
		}
		case '[': {
			return json_parse_impl_parse_array(str);
		}
		case '-': {
			return json_parse_impl_parse_number(str);
		}
		case '"': {
			return json_parse_impl_parse_string(str);
		}
		default: {
			if(first_char >= '0' && first_char <= '9') {
				return json_parse_impl_parse_number(str);
			}

			if(json_parse_impl_is_ws(first_char)) {
				return new_json_parse_result_error(TSTR_STATIC_LIT(
				    "implementation error, skip whitespace, before parsing 'value'"));
			}

			return new_json_parse_result_error(TSTR_STATIC_LIT("invalid value"));
		}
	}
}

NODISCARD JsonParseResult json_variant_parse_from_str(const tstr_view str) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	// JSON-text = ws value ws

	tstr_view current = str;

	json_parse_impl_skip_ws(&current);

	const JsonParseResult result = json_parse_impl_parse_value(&current);

	IF_JSON_PARSE_RESULT_IS_ERROR_IGN(result) {
		return result;
	}

	json_parse_impl_skip_ws(&current);

	if(current.len != 0) {
		return new_json_parse_result_error(
		    TSTR_STATIC_LIT("Didn't reach the end, invalid data at the end"));
	}

	return result;
}

NODISCARD JsonParseResult json_variant_parse_from_file(const tstr str) {

	size_t file_size = 0;

	const void* const file_data = read_entire_file(tstr_cstr(&str), &file_size);

	if(file_data == NULL) {
		return new_json_parse_result_error(TSTR_STATIC_LIT("Error in reading file"));
	}

	const tstr_view str_view =
	    tstr_view_from_readonly_buffer((ReadonlyBuffer){ .data = file_data, .size = file_size });

	return json_variant_parse_from_str(str_view);
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

	// TODO
	UNUSED(json_variant);
	UNUSED(options);

	return result;
}
