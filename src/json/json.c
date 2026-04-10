
#include "./json.h"

#include <utils/path.h>
#include <utils/sized_buffer.h>
#include <utils/string_builder.h>
#include <utils/utf8_helper.h>

#include <math.h>
#include <tmap.h>
#include <tvec.h>
#include <utf8proc.h>

// see: https://datatracker.ietf.org/doc/html/rfc8259

TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(JsonValue)

typedef TVEC_TYPENAME(JsonValue) JsonValueArr;

struct JsonArrayImpl {
	JsonValueArr value;
};

TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(Utf8Codepoint)

typedef TVEC_TYPENAME(Utf8Codepoint) JsonCharArr;

struct JsonStringImpl {
	JsonCharArr value;
};

// keys can only be strings
typedef struct {
	JsonString string;
} JsonObjectKey;

TMAP_DEFINE_AND_IMPLEMENT_MAP_TYPE(JsonObjectKey, JsonObjectKeyName, JsonValue, JsonValueMapImpl)

TMAP_HASH_FUNC_SIG(JsonObjectKey, JsonObjectKeyName) {
	return TMAP_HASH_BYTES(TVEC_DATA_CONST(Utf8Codepoint, &key.string.value),
	                       TVEC_LENGTH(Utf8Codepoint, key.string.value));
}

TMAP_EQ_FUNC_SIG(JsonObjectKey, JsonObjectKeyName) {
	return json_string_eq(&key1.string, &key2.string);
}

typedef TMAP_TYPENAME_MAP(JsonValueMapImpl) JsonValueMap;

struct JsonObjectImpl {
	JsonValueMap value;
};

static void tstr_view_advance_by(tstr_view* const str, size_t amount) {
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

static void source_position_location_process_new_line(SourceLocation* const loc) {
	loc->pos.line++;
	loc->pos.col = 0;
}

static void source_position_location_advance_col_by(SourceLocation* const loc, size_t amount) {
	loc->pos.col += amount;
}

typedef struct {
	tstr_view view;
	SourceLocation loc;
} JsonParseState;

NODISCARD static bool json_parse_state_is_eof(const JsonParseState state) {
	return state.view.len == 0;
}

NODISCARD static size_t json_parse_state_get_str_len(const JsonParseState state) {
	return state.view.len;
}

static void json_parse_state_skip_by(JsonParseState* const state, size_t amount, bool advance_loc) {
	assert(state->view.len >= amount);
	tstr_view_advance_by(&(state->view), amount);
	if(advance_loc) {
		source_position_location_advance_col_by(&(state->loc), amount);
	}
}

NODISCARD static char json_parse_state_get_next_char(JsonParseState* const state) {
	assert(state->view.len > 0);
	const char result = state->view.data[0];
	json_parse_state_skip_by(state, 1, true);
	return result;
}

NODISCARD static char json_parse_state_peek_char_at(const JsonParseState state, size_t index) {
	assert(state.view.len > index);
	return state.view.data[index];
}

NODISCARD static char json_parse_state_peek_next_char(const JsonParseState state) {
	return json_parse_state_peek_char_at(state, 0);
}

#define JSON_NEWLINE_CHAR_FOR_LOCATION '\n'

static void json_parse_impl_skip_ws(JsonParseState* const state) {

	size_t offset = 0;

	while(json_parse_state_get_str_len(*state) > offset) {

		const char value = json_parse_state_peek_char_at(*state, offset);

		if(json_parse_impl_is_ws(value)) {
			// NOTE: only treating newline (\n) as a new line, not \r\n or \r
			if(value == JSON_NEWLINE_CHAR_FOR_LOCATION) {
				source_position_location_process_new_line(&(state->loc));
			} else {
				source_position_location_advance_col_by(&(state->loc), 1);
			}

			++offset;
			continue;
		}

		break;
	}

	if(offset == 0) {
		return;
	}

	json_parse_state_skip_by(state, offset, false);
}

NODISCARD SourceLocation make_null_source_location(void) {
	return (SourceLocation){ .source = new_json_source_file((JsonFileSource){ .file_path = NULL }),
		                     .pos = (SourcePosition){ .line = 0, .col = 0 } };
}

NODISCARD bool is_null_source_location(SourceLocation location) {
	SWITCH_JSON_SOURCE(location.source) {
		CASE_JSON_SOURCE_IS_FILE_CONST(location.source) {
			return file.file_path == NULL;
		}
		VARIANT_CASE_END();
		CASE_JSON_SOURCE_IS_STRING_CONST(location.source) {
			return string.data.data == NULL;
		}
		VARIANT_CASE_END();
		default: {
			return false;
		}
	}
}

NODISCARD static JsonError make_json_error_at(const SourceLocation loc, const tstr_static message) {
	return (JsonError){ .message = message, .loc = loc };
}

NODISCARD static JsonParseResult json_parse_impl_parse_boolean(JsonParseState* const state) {

	// boolean = false / true
	// false = %x66.61.6c.73.65   ; false
	// true  = %x74.72.75.65      ; true

	{
		const tstr_static false_value = TSTR_STATIC_LIT("false");
		if(tstr_view_starts_with(state->view, false_value.ptr)) {

			json_parse_state_skip_by(state, false_value.len, true);

			return new_json_parse_result_ok(
			    new_json_value_boolean((JsonBoolean){ .value = false }));
		}
	}

	{
		const tstr_static true_value = TSTR_STATIC_LIT("true");
		if(tstr_view_starts_with(state->view, true_value.ptr)) {

			json_parse_state_skip_by(state, true_value.len, true);

			return new_json_parse_result_ok(new_json_value_boolean((JsonBoolean){ .value = true }));
		}
	}

	return new_json_parse_result_error(
	    make_json_error_at(state->loc, TSTR_STATIC_LIT("not a boolean")));
}

NODISCARD static JsonParseResult json_parse_impl_parse_null(JsonParseState* const state) {

	//  null  = %x6e.75.6c.6c      ; null

	{
		const tstr_static null_value = TSTR_STATIC_LIT("null");
		if(tstr_view_starts_with(state->view, null_value.ptr)) {

			json_parse_state_skip_by(state, null_value.len, true);

			return new_json_parse_result_ok(new_json_value_null());
		}
	}

	return new_json_parse_result_error(make_json_error_at(state->loc, TSTR_STATIC_LIT("not null")));
}

NODISCARD JsonObject* get_empty_json_object(void) {
	JsonObject* const object = malloc(sizeof(JsonObject));

	if(object == NULL) {
		return NULL;
	}

	object->value = TMAP_EMPTY(JsonValueMapImpl);
	return object;
}

NODISCARD static tstr_static json_object_add_entry_impl(JsonObject* const json_object,
                                                        const JsonObjectKey key,
                                                        const JsonValue value) {

	const TmapInsertResult result =
	    TMAP_INSERT(JsonValueMapImpl, &(json_object->value), key, value, false);

	switch(result) {
		case TmapInsertResultOk: {
			return tstr_static_null();
		}
		case TmapInsertResultErr: {
			return TSTR_STATIC_LIT("json object add error");
		}
		case TmapInsertResultWouldOverwrite: {
			return TSTR_STATIC_LIT("json object has duplicate key");
		}
		default: {
			return TSTR_STATIC_LIT("json object add unknown error");
		}
	}
}

NODISCARD static JsonObjectKey release_json_string_into_object_key(JsonString** const string) {

	const JsonObjectKey result = { .string = **string };

	free(*string);

	*string = NULL;

	return result;
}

NODISCARD tstr_static json_object_add_entry(JsonObject* const json_object,
                                            JsonString** const key_moved, const JsonValue value) {

	const JsonObjectKey key = release_json_string_into_object_key(key_moved);

	return json_object_add_entry_impl(json_object, key, value);
}

NODISCARD tstr_static json_object_add_entry_tstr(JsonObject* const json_object, const tstr* key,
                                                 const JsonValue value) {
	JsonString* key_string = json_get_string_from_tstr(key);

	if(key_string == NULL) {
		return TSTR_STATIC_LIT("OOM");
	}

	return json_object_add_entry(json_object, &key_string, value);
}

NODISCARD tstr_static json_object_add_entry_cstr(JsonObject* json_object, const char* key,
                                                 JsonValue value) {
	JsonString* key_string = json_get_string_from_cstr(key);

	if(key_string == NULL) {
		return TSTR_STATIC_LIT("OOM");
	}

	return json_object_add_entry(json_object, &key_string, value);
}

NODISCARD static inline JsonError json_error_none(const SourceLocation loc) {
	return (JsonError){ .message = tstr_static_null(), .loc = loc };
}

NODISCARD static bool json_error_is_none(const JsonError error) {
	return tstr_static_is_null(error.message);
}

NODISCARD static JsonParseResult json_parse_impl_parse_string(JsonParseState* state);

NODISCARD static JsonParseResult json_parse_impl_parse_value(JsonParseState* state);

static void free_json_key(JsonObjectKey* key);

NODISCARD static JsonError json_parse_impl_parse_object_member(JsonParseState* const state,
                                                               JsonObject* const json_object) {
	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	// member = string name-separator value

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(
		    state->loc,
		    TSTR_STATIC_LIT("empty object member: missing member after 'value-separator'"));
	}

	const JsonParseResult string_result = json_parse_impl_parse_string(state);

	IF_JSON_PARSE_RESULT_IS_ERROR_CONST(string_result) {
		return error;
	}

	const JsonValue key_raw = json_parse_result_get_as_ok(string_result);

	IF_JSON_VALUE_IS_NOT_STRING(key_raw) {
		return make_json_error_at(
		    state->loc,
		    TSTR_STATIC_LIT("implementation error: string parser didn't return a string"));
	}

	JsonString* key = json_value_get_as_string(key_raw);

#define FREE_AT_END() \
	do { \
		free_json_string(key); \
	} while(false)

	{
		// name-separator  = ws %x3A ws  ; : colon

		json_parse_impl_skip_ws(state);

		if(json_parse_state_is_eof(*state)) {
			FREE_AT_END();
			return make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT("empty object member: missing 'name-separator' after member name"));
		}

		const char next_value = json_parse_state_peek_next_char(*state);

		if(next_value != ':') {
			FREE_AT_END();
			return make_json_error_at(state->loc, TSTR_STATIC_LIT("wrong name-separator"));
		}

		json_parse_state_skip_by(state, 1, true);

		if(json_parse_state_is_eof(*state)) {
			return make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT("empty object member: mssing value after 'name-separator'"));
		}

		json_parse_impl_skip_ws(state);
	}

	const JsonParseResult value_result = json_parse_impl_parse_value(state);

	IF_JSON_PARSE_RESULT_IS_ERROR_CONST(value_result) {
		FREE_AT_END();
		return error;
	}

	JsonValue value = json_parse_result_get_as_ok(value_result);

#undef FREE_AT_END

	JsonObjectKey correct_key = release_json_string_into_object_key(&key);

#define FREE_AT_END() \
	do { \
		free_json_key(&correct_key); \
		free_json_value(&value); \
	} while(false)

	const tstr_static add_result = json_object_add_entry_impl(json_object, correct_key, value);

	if(!tstr_static_is_null(add_result)) {

		FREE_AT_END();
		return make_json_error_at(state->loc, add_result);
	}

	return json_error_none(state->loc);
}

#undef FREE_AT_END

NODISCARD static JsonParseResult json_parse_impl_parse_object(JsonParseState* const state) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2

	//       object = begin-object [ member *( value-separator member ) ] end-object
	// member = string name-separator value
	// begin-object    = ws %x7B ws  ; { left curly bracket
	// end-object      = ws %x7D ws  ; } right curly bracket
	// name-separator  = ws %x3A ws  ; : colon
	// value-separator = ws %x2C ws  ; , comma

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty object: missing 'begin-object'")));
	}

	{ // begin-object

		json_parse_impl_skip_ws(state);

		if(json_parse_state_is_eof(*state)) {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty object: missing '{'")));
		}

		const char next_value = json_parse_state_peek_next_char(*state);

		if(next_value != '{') {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("wrong begin-object")));
		}

		json_parse_state_skip_by(state, 1, true);

		json_parse_impl_skip_ws(state);

		if(json_parse_state_is_eof(*state)) {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty object: <EOF> after '{'")));
		}
	}

	// either member or a end-object

	json_parse_impl_skip_ws(state);

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state->loc,
		    TSTR_STATIC_LIT(
		        "empty object: missing 'member' or 'end-object' after 'begin-object'")));
	}

	const char next_char = json_parse_state_peek_next_char(*state);

	if(next_char == '}') {
		// end-object

		json_parse_state_skip_by(state, 1, true);

		json_parse_impl_skip_ws(state);

		JsonObject* const object = get_empty_json_object();

		if(object == NULL) {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("OOM")));
		}

		return new_json_parse_result_ok(new_json_value_object(object));
	}

	JsonObject* const object = get_empty_json_object();

	if(object == NULL) {
		return new_json_parse_result_error(make_json_error_at(state->loc, TSTR_STATIC_LIT("OOM")));
	}

#define FREE_AT_END() \
	do { \
		free_json_object(object); \
	} while(false)

	{
		// member *( value-separator member )

		const JsonError member_result = json_parse_impl_parse_object_member(state, object);

		if(!json_error_is_none(member_result)) {
			FREE_AT_END();
			return new_json_parse_result_error(member_result);
		}

		while(true) {
			// end-object or value-separator

			json_parse_impl_skip_ws(state);

			if(json_parse_state_is_eof(*state)) {
				FREE_AT_END();
				return new_json_parse_result_error(make_json_error_at(
				    state->loc, TSTR_STATIC_LIT("empty object: missing 'member' or 'end-object'")));
			}

			const char end_char = json_parse_state_peek_next_char(*state);

			if(end_char == '}') {
				// end-object

				json_parse_state_skip_by(state, 1, true);

				json_parse_impl_skip_ws(state);

				return new_json_parse_result_ok(new_json_value_object(object));
			}

			if(end_char != ',') {
				FREE_AT_END();
				return new_json_parse_result_error(make_json_error_at(
				    state->loc, TSTR_STATIC_LIT("invalid continuation of member in object")));
			}

			{
				// value-separator

				json_parse_state_skip_by(state, 1, true);

				json_parse_impl_skip_ws(state);
			}

			const JsonError member_result_2 = json_parse_impl_parse_object_member(state, object);

			if(!json_error_is_none(member_result_2)) {
				FREE_AT_END();
				return new_json_parse_result_error(member_result_2);
			}
		}
	}
}

#undef FREE_AT_END

NODISCARD JsonArray* get_empty_json_array(void) {
	JsonArray* const array = malloc(sizeof(JsonArray));

	if(array == NULL) {
		return NULL;
	}

	array->value = TVEC_EMPTY(JsonValue);
	return array;
}

NODISCARD tstr_static json_array_add_entry(JsonArray* const json_array, const JsonValue entry) {

	const TvecResult result = TVEC_PUSH(JsonValue, &(json_array->value), entry);

	if(result != TvecResultOk) {
		return TSTR_STATIC_LIT("json array add error");
	}

	return tstr_static_null();
}

NODISCARD static JsonError json_parse_impl_parse_array_value(JsonParseState* const state,
                                                             JsonArray* const json_array) {
	// just parsers a value and than adds it to the array

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(state->loc, TSTR_STATIC_LIT("empty array value"));
	}

	const JsonParseResult value_result = json_parse_impl_parse_value(state);

	IF_JSON_PARSE_RESULT_IS_ERROR_CONST(value_result) {
		return error;
	}

	JsonValue value = json_parse_result_get_as_ok(value_result);

#undef FREE_AT_END
#define FREE_AT_END() \
	do { \
		free_json_value(&value); \
	} while(false)

	const tstr_static add_result = json_array_add_entry(json_array, value);

	if(!tstr_static_is_null(add_result)) {
		FREE_AT_END();
		return make_json_error_at(state->loc, add_result);
	}

	return json_error_none(state->loc);
}

#undef FREE_AT_END

NODISCARD static JsonParseResult json_parse_impl_parse_array(JsonParseState* const state) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2

	//       array = begin-array [ value *( value-separator value ) ] end-array
	// begin-array     = ws %x5B ws  ; [ left square bracket
	// end-array       = ws %x5D ws  ; ] right square bracket
	// value-separator = ws %x2C ws  ; , comma

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(
		    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty array: missing 'begin-array'")));
	}

	{ // begin-array

		json_parse_impl_skip_ws(state);

		if(json_parse_state_is_eof(*state)) {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty array: missing '['")));
		}

		const char next_value = json_parse_state_peek_next_char(*state);

		if(next_value != '[') {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("wrong begin-array")));
		}

		json_parse_state_skip_by(state, 1, true);

		json_parse_impl_skip_ws(state);

		if(json_parse_state_is_eof(*state)) {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty array: <EOF> after '['")));
		}
	}

	// either end-array or value

	json_parse_impl_skip_ws(state);

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state->loc,
		    TSTR_STATIC_LIT("empty array: missing 'value' or 'end-array' after 'begin-array'")));
	}

	const char next_char = json_parse_state_peek_next_char(*state);

	if(next_char == ']') {
		// end-array

		json_parse_state_skip_by(state, 1, true);

		json_parse_impl_skip_ws(state);

		JsonArray* const array = get_empty_json_array();

		if(array == NULL) {
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("OOM")));
		}

		return new_json_parse_result_ok(new_json_value_array(array));
	}

	JsonArray* const array = get_empty_json_array();

	if(array == NULL) {
		return new_json_parse_result_error(make_json_error_at(state->loc, TSTR_STATIC_LIT("OOM")));
	}

#define FREE_AT_END() \
	do { \
		free_json_array(array); \
	} while(false)

	{
		// value *( value-separator value )

		const JsonError value_result = json_parse_impl_parse_array_value(state, array);

		if(!json_error_is_none(value_result)) {
			FREE_AT_END();
			return new_json_parse_result_error(value_result);
		}

		while(true) {
			// end-array or value-separator

			json_parse_impl_skip_ws(state);

			if(json_parse_state_is_eof(*state)) {
				FREE_AT_END();
				return new_json_parse_result_error(make_json_error_at(
				    state->loc, TSTR_STATIC_LIT("empty array: missing 'value' or 'end-array'")));
			}

			const char end_char = json_parse_state_peek_next_char(*state);

			if(end_char == ']') {
				// end-array

				json_parse_state_skip_by(state, 1, true);

				json_parse_impl_skip_ws(state);

				return new_json_parse_result_ok(new_json_value_array(array));
			}

			if(end_char != ',') {
				FREE_AT_END();
				return new_json_parse_result_error(make_json_error_at(
				    state->loc, TSTR_STATIC_LIT("invalid continuation of values in array")));
			}

			{
				// value-separator

				json_parse_state_skip_by(state, 1, true);

				json_parse_impl_skip_ws(state);
			}

			const JsonError value_result_2 = json_parse_impl_parse_array_value(state, array);

			if(!json_error_is_none(value_result_2)) {
				FREE_AT_END();
				return new_json_parse_result_error(value_result_2);
			}
		}
	}
}

#undef FREE_AT_END

NODISCARD static JsonError json_parse_impl_parse_number_int_part(JsonParseState* const state,
                                                                 double* const out_result) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	//     int = zero / ( digit1-9 *DIGIT )
	// digit1-9 = %x31-39         ; 1-9
	// zero = %x30                ; 0

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(state->loc, TSTR_STATIC_LIT("empty number int part"));
	}

	const char first_value = json_parse_state_peek_next_char(*state);

	if(first_value == '0') {
		json_parse_state_skip_by(state, 1, true);
		*out_result = 0;
		return json_error_none(state->loc);
	}

	if(first_value > '9' || first_value < '1') {
		return make_json_error_at(state->loc,
		                          TSTR_STATIC_LIT("invalid number int part: incorrect start"));
	}

	uint64_t value = (first_value - '0');
	json_parse_state_skip_by(state, 1, true);

	while(true) {
		if(json_parse_state_is_eof(*state)) {
			break;
		}

		const char next_value = json_parse_state_peek_next_char(*state);

		if(next_value > '9' || next_value < '0') {
			break;
		}

		const uint64_t previous_value = value;

		value = (value * 10) + (next_value - '0');
		json_parse_state_skip_by(state, 1, true);

		if(previous_value > value) {
			// overflow detected
			return make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT("invalid number int part: value overflowed a 64 bit number!"));
		}
	}

	// TODO: JSON overflows much earlier, Number.MAX_SAFE_INTEGER in js says, but is this in the
	// spec?

	// TODO: check if this has rounding errors!
	*out_result = (double)value;
	return json_error_none(state->loc);
}

NODISCARD static JsonError json_parse_impl_parse_number_frac_part(JsonParseState* const state,
                                                                  double* const out_result) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	//     frac = decimal-point 1*DIGIT
	// decimal-point = %x2E       ; .

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty number frac part: expected '.' but got eof"));
	}

	const char next_char = json_parse_state_peek_next_char(*state);

	if(next_char != '.') {
		return make_json_error_at(state->loc,
		                          TSTR_STATIC_LIT("wrong number frac part: missing starting '.'"));
	}

	json_parse_state_skip_by(state, 1, true);

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(state->loc,
		                          TSTR_STATIC_LIT("empty number frac part: <EOF> after '.'"));
	}

	const char first_value = json_parse_state_peek_next_char(*state);

	if(first_value > '9' || first_value < '0') {
		return make_json_error_at(state->loc,
		                          TSTR_STATIC_LIT("invalid number frac part: incorrect start"));
	}

	double divider = 10.0;
	double value = ((double)(first_value - '0')) / divider;
	json_parse_state_skip_by(state, 1, true);

	while(true) {
		if(json_parse_state_is_eof(*state)) {
			break;
		}

		const char next_value = json_parse_state_peek_next_char(*state);

		if(next_value > '9' || next_value < '0') {
			break;
		}

		divider = divider * 10.0;
		value = value + (((double)next_value - '0') / divider);
		json_parse_state_skip_by(state, 1, true);
	}

	*out_result = value;
	return json_error_none(state->loc);
}

NODISCARD static JsonError json_parse_impl_parse_number_exp_part(JsonParseState* const state,
                                                                 int64_t* const out_result) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2.
	//      exp = e [ minus / plus ] 1*DIGIT
	// digit1-9 = %x31-39         ; 1-9
	// e = %x65 / %x45            ; e E
	// minus = %x2D               ; -
	// plus = %x2B                ; +

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty number exp part: expected 'e' or 'E' but got eof"));
	}

	const char next_char = json_parse_state_peek_next_char(*state);

	if(next_char != 'e' && next_char != 'E') {
		return make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("wrong number exp part: missing starting 'e' / 'E'"));
	}
	json_parse_state_skip_by(state, 1, true);

	bool minus = false;

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(state->loc,
		                          TSTR_STATIC_LIT("empty number exp part: <EOF> after 'e'"));
	}

	{
		const char first_value = json_parse_state_peek_next_char(*state);

		if(first_value == '+') {
			minus = false;
			json_parse_state_skip_by(state, 1, true);
		} else if(first_value == '-') {
			minus = true;
			json_parse_state_skip_by(state, 1, true);
		}
	}

	if(json_parse_state_is_eof(*state)) {
		return make_json_error_at(
		    state->loc,
		    TSTR_STATIC_LIT("empty number exp part: no values after 'e' and optional sign"));
	}

	const char first_value = json_parse_state_peek_next_char(*state);

	if(first_value > '9' || first_value < '0') {
		return make_json_error_at(state->loc,
		                          TSTR_STATIC_LIT("invalid number exp part: incorrect start"));
	}

	int64_t value = (first_value - '0');
	json_parse_state_skip_by(state, 1, true);

	while(true) {
		if(json_parse_state_is_eof(*state)) {
			break;
		}

		const char next_value = json_parse_state_peek_next_char(*state);

		if(next_value > '9' || next_value < '0') {
			break;
		}

		const int64_t previous_value = value;

		value = (value * 10) + (next_value - '0');
		json_parse_state_skip_by(state, 1, true);

		if(previous_value > value) {
			// overflow detected
			return make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT("invalid number exp part: value overflowed a 64 bit number!"));
		}
	}

	// TODO: JSON overflows much earlier, Number.MAX_SAFE_INTEGER in js says, but is this in the
	// spec?

	// TODO: check if this overflow when using -
	*out_result = minus ? -(value) : value;
	return json_error_none(state->loc);
}

NODISCARD static double get_power_of_10(uint64_t value) {
	// TODO: find a faster way than this
	return pow(10.0, (double)value);
}

NODISCARD static double json_number_make_value_int_exp(double int_value, int64_t exp) {

	if(exp == 0) {
		return int_value;
	}

	if(exp < 0) {

		return int_value / get_power_of_10((uint64_t)(-exp));
	}

	return int_value * get_power_of_10(exp);
}

NODISCARD static JsonParseResult json_parse_impl_parse_number(JsonParseState* const state) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	//     number = [ minus ] int [ frac ] [ exp ]
	// decimal-point = %x2E       ; .
	// digit1-9 = %x31-39         ; 1-9
	// e = %x65 / %x45            ; e E
	// exp = e [ minus / plus ] 1*DIGIT
	// frac = decimal-point 1*DIGIT
	// int = zero / ( digit1-9 *DIGIT )
	// minus = %x2D               ; -
	// plus = %x2B                ; +
	// zero = %x30                ; 0

	bool minus = false;

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty number: expected number-start but got eof")));
	}

	const char minus_char = json_parse_state_peek_next_char(*state);

	if(minus_char == '-') {
		minus = true;
		json_parse_state_skip_by(state, 1, true);
	}

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(
		    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty number: <EOF> after '-'")));
	}

	double int_value = 0.0;

	const JsonError int_result = json_parse_impl_parse_number_int_part(state, &int_value);

	if(!json_error_is_none(int_result)) {
		return new_json_parse_result_error(int_result);
	}

#define JSON_NUMBER_FROM_MINUS_INT() \
	{ \
		.value = minus ? -(int_value) : int_value \
	}

	// have: minus + int
	// <EOF> after that
	if(json_parse_state_is_eof(*state)) {
		const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT();
		return new_json_parse_result_ok(new_json_value_number(number));
	}

	const char next_value = json_parse_state_peek_next_char(*state);

	double frac = 0.0;
	int64_t exp = 1;

	bool saw_frac = false;
	bool saw_exp = false;

	if(next_value == '.') {
		// frac

		saw_frac = true;

		const JsonError frac_result = json_parse_impl_parse_number_frac_part(state, &frac);

		if(!json_error_is_none(frac_result)) {
			return new_json_parse_result_error(frac_result);
		}
	} else if(next_value == 'e' || next_value == 'E') {
		// exp

		saw_exp = true;

		const JsonError exp_result = json_parse_impl_parse_number_exp_part(state, &exp);

		if(!json_error_is_none(exp_result)) {
			return new_json_parse_result_error(exp_result);
		}

	} else {
		// have: minus + int
		// no frac or exp at the end

		const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT();
		return new_json_parse_result_ok(new_json_value_number(number));
	}

	// saw frac or exp
	assert(saw_frac || saw_exp);

	// are eof
	if(json_parse_state_is_eof(*state)) {

		if(saw_frac) {

#define JSON_NUMBER_FROM_MINUS_INT_FRAC() \
	{ \
		.value = (minus ? -(int_value + frac) : int_value + frac) \
	}

			// have: minus + int + frac
			const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT_FRAC();
			return new_json_parse_result_ok(new_json_value_number(number));
		}

		if(saw_exp) {

#define JSON_NUMBER_FROM_MINUS_INT_EXP() \
	{ \
		.value = (minus ? -(json_number_make_value_int_exp(int_value, exp)) \
		                : json_number_make_value_int_exp(int_value, exp)) \
	}

			// have: minus + int + exp
			const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT_EXP();
			return new_json_parse_result_ok(new_json_value_number(number));
		}

		return new_json_parse_result_error(make_json_error_at(
		    state->loc,
		    TSTR_STATIC_LIT("implementation error in int + frac + exp number parsing")));
	}

	// we are already finished
	if(saw_exp) {
		assert(!saw_frac);

		// have: minus + int + exp
		const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT_EXP();
		return new_json_parse_result_ok(new_json_value_number(number));
	}

	const char next_value2 = json_parse_state_peek_next_char(*state);

	if(next_value2 == 'e' || next_value2 == 'E') {
		// exp

		saw_exp = true;

		const JsonError exp_result = json_parse_impl_parse_number_exp_part(state, &exp);

		if(!json_error_is_none(exp_result)) {
			return new_json_parse_result_error(exp_result);
		}
	} else {
		assert(saw_frac);
		// have: minus + int + frac
		const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT_FRAC();
		return new_json_parse_result_ok(new_json_value_number(number));
	}

	assert(saw_exp && saw_frac);

#define JSON_NUMBER_FROM_MINUS_INT_FRAC_EXP() \
	{ \
		.value = (minus ? -(json_number_make_value_int_exp(int_value + frac, exp)) \
		                : json_number_make_value_int_exp(int_value + frac, exp)) \
	}

	// have: minus + int + frac + exp
	const JsonNumber number = JSON_NUMBER_FROM_MINUS_INT_FRAC_EXP();
	return new_json_parse_result_ok(new_json_value_number(number));
}

NODISCARD static JsonString* get_empty_json_string_impl(void) {
	JsonString* const string = malloc(sizeof(JsonString));

	if(string == NULL) {
		return NULL;
	}

	string->value = TVEC_EMPTY(Utf8Codepoint);
	return string;
}

NODISCARD static tstr_static json_string_add_char_impl(JsonString* const json_string,
                                                       const Utf8Codepoint codepoint) {

	const TvecResult result = TVEC_PUSH(Utf8Codepoint, &(json_string->value), codepoint);

	if(result != TvecResultOk) {
		return TSTR_STATIC_LIT("json string add error");
	}

	return tstr_static_null();
}

GENERATE_VARIANT_ALL_UTF8_NEXT_CHAR_RESULT()

NODISCARD static Utf8NextCharResult utf8_get_next_char_and_consume(JsonParseState* const state) {

	if(json_parse_state_is_eof(*state)) {
		return new_utf8_next_char_result_error(make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty string: <EOF> when getting next char")));
	}

	utf8proc_int32_t codepoint = 0;
	utf8proc_ssize_t result = utf8proc_iterate(
	    (const utf8proc_uint8_t*)((const void*)state->view.data), state->view.len, &codepoint);

	if(result < 0) {
		return new_utf8_next_char_result_error(
		    make_json_error_at(state->loc, tstr_static_from_static_cstr(utf8proc_errmsg(result))));
	}

	if(result == 0) {
		return new_utf8_next_char_result_error(
		    make_json_error_at(state->loc, TSTR_STATIC_LIT("invalid codepoint length")));
	}
	json_parse_state_skip_by(state, result, true);

	return new_utf8_next_char_result_ok(codepoint);
}

NODISCARD static JsonParseResult json_parse_impl_parse_string(JsonParseState* const state) {
	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	//       string = quotation-mark *char quotation-mark
	//  char = unescaped /
	//      escape (
	//          %x22 /          ; "    quotation mark  U+0022
	//          %x5C /          ; \    reverse solidus U+005C
	//          %x2F /          ; /    solidus         U+002F
	//          %x62 /          ; b    backspace       U+0008
	//          %x66 /          ; f    form feed       U+000C
	//          %x6E /          ; n    line feed       U+000A
	//          %x72 /          ; r    carriage return U+000D
	//          %x74 /          ; t    tab             U+0009
	//          %x75 4HEXDIG )  ; uXXXX                U+XXXX
	//  escape = %x5C              ; \
	//  quotation-mark = %x22      ; "
	//  unescaped = %x20-21 / %x23-5B / %x5D-10FFFF

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty string: expected '\"' but got eof")));
	}

	const char next_char = json_parse_state_peek_next_char(*state);

	if(next_char != '"') {
		return new_json_parse_result_error(
		    make_json_error_at(state->loc, TSTR_STATIC_LIT("wrong quotation-mark")));
	}
	json_parse_state_skip_by(state, 1, true);

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(
		    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty string: <EOF> after '\"'")));
	}

	JsonString* const string = get_empty_json_string_impl();

#define FREE_AT_END() \
	do { \
		free_json_string(string); \
	} while(false)

	while(true) {

		if(json_parse_state_is_eof(*state)) {
			return new_json_parse_result_error(make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT("empty string: expected '\"' or string-char but got eof")));
		}

		const Utf8NextCharResult result = utf8_get_next_char_and_consume(state);

		IF_UTF8_NEXT_CHAR_RESULT_IS_ERROR_CONST(result) {
			FREE_AT_END();
			return new_json_parse_result_error(error);
		}

		Utf8Codepoint codepoint = utf8_next_char_result_get_as_ok(result);

		if(codepoint < 0) {
			FREE_AT_END();
			return new_json_parse_result_error(make_json_error_at(
			    state->loc, TSTR_STATIC_LIT("invalid string char: range (-inf, 0)")));
		} else if(codepoint >= 0 && codepoint < 0x20) {
			FREE_AT_END();

			static_assert(JSON_NEWLINE_CHAR_FOR_LOCATION >= 0 &&
			              JSON_NEWLINE_CHAR_FOR_LOCATION <= 0x020);

			return new_json_parse_result_error(make_json_error_at(
			    state->loc, TSTR_STATIC_LIT("invalid string char: range [0, 0x20)")));
		} else if(codepoint >= 0x20 && codepoint <= 0x21) {
			goto add_codepoint_raw;
		} else if(codepoint == '"') {
			static_assert(0x22 == '"');
			break;
		} else if(codepoint >= 0x23 && codepoint <= 0x5B) {
			goto add_codepoint_raw;
		} else if(codepoint == '\\') {
			static_assert(0x5C == '\\');
			goto escape_logic;
		} else if(codepoint >= 0x5D && codepoint <= 0x10FFFF) {
			goto add_codepoint_raw;
		} else {
			FREE_AT_END();
			return new_json_parse_result_error(make_json_error_at(
			    state->loc, TSTR_STATIC_LIT("invalid string char: range (0x10FFFF, +inf)")));
		}
		//

		continue;
		//
	add_codepoint_raw:
		const tstr_static string_add_result = json_string_add_char_impl(string, codepoint);
		if(!tstr_static_is_null(string_add_result)) {
			FREE_AT_END();
			return new_json_parse_result_error(make_json_error_at(state->loc, string_add_result));
		}

		continue;
	//
	escape_logic:
		// NOTE. next char has to be ascii!

		if(json_parse_state_is_eof(*state)) {
			FREE_AT_END();
			return new_json_parse_result_error(
			    make_json_error_at(state->loc, TSTR_STATIC_LIT("empty string escape sequence")));
		}

		const char escape_char = json_parse_state_get_next_char(state);

		if(escape_char == '"') {
			static_assert(0x22 == '"');
			codepoint = '"';
			goto add_codepoint_raw;
		} else if(escape_char == '\\') {
			static_assert(0x5C == '\\');
			codepoint = '\\';
			goto add_codepoint_raw;
		} else if(escape_char == '/') {
			static_assert(0x2F == '/');
			codepoint = '/';
			goto add_codepoint_raw;
		} else if(escape_char == 'b') {
			static_assert(0x62 == 'b');
			static_assert(0x08 == '\b');
			codepoint = '\b';
			goto add_codepoint_raw;
		} else if(escape_char == 'f') {
			static_assert(0x66 == 'f');
			static_assert(0x0C == '\f');
			codepoint = '\f';
			goto add_codepoint_raw;
		} else if(escape_char == 'n') {
			static_assert(0x6E == 'n');
			static_assert(0x0A == '\n');
			codepoint = '\n';
			goto add_codepoint_raw;
		} else if(escape_char == 'r') {
			static_assert(0x72 == 'r');
			static_assert(0x0D == '\r');
			codepoint = '\r';
			goto add_codepoint_raw;
		} else if(escape_char == 't') {
			static_assert(0x74 == 't');
			static_assert(0x09 == '\t');
			codepoint = '\t';
			goto add_codepoint_raw;
		} else if(escape_char == 'u') {
			static_assert(0x75 == 'u');

			if(json_parse_state_get_str_len(*state) < 4) {
				FREE_AT_END();
				return new_json_parse_result_error(make_json_error_at(
				    state->loc,
				    TSTR_STATIC_LIT(
				        "invalid string escape sequence: unicode escape is missing values, it "
				        "requires at least 4 chars after it")));
			}

			Utf8Codepoint composed_codepoint = 0;

			for(size_t i = 0; i < 4; ++i) {
				const char value = json_parse_state_peek_char_at(*state, i);

				uint8_t num = 0;

				if(value >= '0' && value <= '9') {
					num = value - '0';
				} else if(value >= 'A' && value <= 'F') {
					num = (value - 'A') + 10;
				} else if(value >= 'a' && value <= 'f') {
					num = (value - 'a') + 10;
				} else {
					FREE_AT_END();
					return new_json_parse_result_error(make_json_error_at(
					    state->loc,
					    TSTR_STATIC_LIT(
					        "invalid string escape sequence: unicode escape has invalid digits")));
				}

				composed_codepoint = (composed_codepoint * 0x10) + num;
			}
			json_parse_state_skip_by(state, 4, true);

			codepoint = composed_codepoint;

			goto add_codepoint_raw;
		} else {
			FREE_AT_END();
			return new_json_parse_result_error(make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT("invalid string escape sequence: not a recognized escape char")));
		}
	}

	return new_json_parse_result_ok(new_json_value_string(string));
}

#undef FREE_AT_END

NODISCARD static JsonParseResult json_parse_impl_parse_value(JsonParseState* const state) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	//       value = false / null / true / object / array / number / string

	if(json_parse_state_is_eof(*state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state->loc, TSTR_STATIC_LIT("empty value: expected value but got eof")));
	}

	const char first_char = json_parse_state_peek_next_char(*state);
	switch(first_char) {
		case 'f': {
			return json_parse_impl_parse_boolean(state);
		}
		case 'n': {
			return json_parse_impl_parse_null(state);
		}
		case 't': {
			return json_parse_impl_parse_boolean(state);
		}
		case '{': {
			return json_parse_impl_parse_object(state);
		}
		case '[': {
			return json_parse_impl_parse_array(state);
		}
		case '-': {
			return json_parse_impl_parse_number(state);
		}
		case '"': {
			return json_parse_impl_parse_string(state);
		}
		default: {
			if(first_char >= '0' && first_char <= '9') {
				return json_parse_impl_parse_number(state);
			}

			if(json_parse_impl_is_ws(first_char)) {
				return new_json_parse_result_error(make_json_error_at(
				    state->loc,
				    TSTR_STATIC_LIT(
				        "implementation error, skip whitespace, before parsing 'value'")));
			}

			return new_json_parse_result_error(make_json_error_at(
			    state->loc,
			    TSTR_STATIC_LIT(
			        "invalid value: no value type was detected based on the start-char")));
		}
	}
}

NODISCARD static JsonParseResult json_value_parse_from_str_impl(const JsonParseState state_const) {

	// see: https://datatracker.ietf.org/doc/html/rfc8259#section-2
	// JSON-text = ws value ws

	JsonParseState state = state_const;

	json_parse_impl_skip_ws(&state);

	const JsonParseResult result = json_parse_impl_parse_value(&state);

	IF_JSON_PARSE_RESULT_IS_ERROR_IGN(result) {
		return result;
	}

	json_parse_impl_skip_ws(&state);

	if(!json_parse_state_is_eof(state)) {
		return new_json_parse_result_error(make_json_error_at(
		    state.loc, TSTR_STATIC_LIT("Didn't reach the end, invalid data at the end")));
	}

	return result;
}

NODISCARD JsonParseResult json_value_parse_from_str(const tstr_view data) {
	const JsonParseState state = {
		.view = data,
		.loc =
		    (SourceLocation){ .source = new_json_source_string((JsonStringSource){ .data = data }),
		                      .pos = (SourcePosition){ .line = 0, .col = 0 } }
	};

	return json_value_parse_from_str_impl(state);
}

NODISCARD JsonParseResult json_value_parse_from_file(const tstr* const file_path) {

	size_t file_size = 0;

	const void* const file_data = read_entire_file(tstr_cstr(file_path), &file_size);

	if(file_data == NULL) {
		return new_json_parse_result_error(make_json_error_at(
		    make_null_source_location(), TSTR_STATIC_LIT("Error in reading file")));
	}

	const tstr_view str_view =
	    tstr_view_from_readonly_buffer((ReadonlyBuffer){ .data = file_data, .size = file_size });

	const JsonParseState state = { .view = str_view,
		                           .loc = (SourceLocation){
		                               .source = new_json_source_file(
		                                   (JsonFileSource){ .file_path = file_path }),
		                               .pos = (SourcePosition){ .line = 0, .col = 0 } } };

	return json_value_parse_from_str_impl(state);
}

void static free_json_string_impl(JsonString* const json_string) {
	TVEC_FREE(Utf8Codepoint, &(json_string->value));
}

void static free_json_string_malloced(JsonString* const json_string) {
	free_json_string_impl(json_string);
	free(json_string);
}

void free_json_string(JsonString* const json_string) {
	free_json_string_malloced(json_string);
}

static void free_json_key(JsonObjectKey* const key) {
	free_json_string_impl(&(key->string));
}

void free_json_object(JsonObject* const json_obj) { // NOLINT(misc-no-recursion)
	TMAP_TYPENAME_ITER(JsonValueMapImpl)
	iter = TMAP_ITER_INIT(JsonValueMapImpl, &(json_obj->value));

	TMAP_TYPENAME_ENTRY(JsonValueMapImpl) value;

	while(TMAP_ITER_NEXT(JsonValueMapImpl, &iter, &value)) {

		free_json_value(&value.value);
		free_json_key(&value.key);
	}

	TMAP_FREE(JsonValueMapImpl, &(json_obj->value));
	free(json_obj);
}

void free_json_array(JsonArray* const json_arr) { // NOLINT(misc-no-recursion)
	for(size_t i = 0; i < TVEC_LENGTH(JsonValue, json_arr->value); ++i) {
		JsonValue* const value = TVEC_GET_AT_MUT(JsonValue, &(json_arr->value), i);
		free_json_value(value);
	}
	TVEC_FREE(JsonValue, &(json_arr->value));
	free(json_arr);
}

void free_json_value(JsonValue* const json_value) {
	SWITCH_JSON_VALUE(*json_value) {
		CASE_JSON_VALUE_IS_OBJECT_CONST(*json_value) {
			free_json_object(object.obj);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_ARRAY_CONST(*json_value) {
			free_json_array(array.arr);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_NUMBER_IGN() {}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_STRING_CONST(*json_value) {
			free_json_string(string);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_BOOLEAN_IGN() {}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_NULL() {}
		break;
		VARIANT_CASE_END();
		default: {
			break;
		}
	}

	*json_value = new_json_value_null();
}

NODISCARD tstr json_value_to_string(const JsonValue json_value) {
	return json_value_to_string_advanced(json_value, (JsonSerializeOptions){ .indent_size = 0 });
}

static void json_to_string_null_impl(StringBuilder* const sb, const JsonSerializeOptions options) {
	UNUSED(options);
	string_builder_append_tstr_static(sb, TSTR_STATIC_LIT("null"));
}

static void json_to_string_boolean_impl(StringBuilder* const sb, const JsonBoolean json_boolean,
                                        const JsonSerializeOptions options) {
	UNUSED(options);
	if(json_boolean.value) {
		string_builder_append_tstr_static(sb, TSTR_STATIC_LIT("true"));
	} else {
		string_builder_append_tstr_static(sb, TSTR_STATIC_LIT("false"));
	}
}

static void json_to_string_number_impl(StringBuilder* const sb, const JsonNumber json_number,
                                       const JsonSerializeOptions options) {

	UNUSED(options);
	// TODO: use better formatting
	STRING_BUILDER_APPENDF(sb, OOM_ASSERT(false, "error in formatting json number");
	                       , "%f", json_number.value);
}

// from my project ass_parser_c, modified slightly
#define CHUNK_SIZE_NORMALIZE 256

static tstr get_normalized_string_from_codepoints(JsonCharArr codepoints) {
	if(codepoints.data == NULL) {
		return tstr_null();
	}

	size_t buffer_size = CHUNK_SIZE_NORMALIZE;
	uint8_t* buffer = (uint8_t*)malloc(buffer_size);

	size_t current_size = 0;

	if(!buffer) {
		return tstr_null();
	}

	for(size_t i = 0; i < TVEC_LENGTH(Utf8Codepoint, codepoints); ++i) {

		if(buffer_size - current_size < 4) {
			buffer_size = buffer_size + CHUNK_SIZE_NORMALIZE;
			uint8_t* new_buffer = (uint8_t*)realloc(buffer, buffer_size);

			if(!new_buffer) {
				free(buffer);
				return tstr_null();
			}

			buffer = new_buffer;
		}

		long result = utf8proc_encode_char(codepoints.data[i], buffer + current_size);

		if(result <= 0) {
			free(buffer);
			return tstr_null();
		}

		current_size = current_size + result;
	}

	if(buffer_size - current_size < 1) {
		buffer_size = buffer_size + 1;
		uint8_t* new_buffer = (uint8_t*)realloc(buffer, buffer_size);

		if(!new_buffer) {
			free(buffer);
			return tstr_null();
		}

		buffer = new_buffer;
	}

	buffer[current_size] = '\0';

	return tstr_own((char*)buffer, current_size, current_size);
}

static tstr get_tstr_from_json_string(const JsonString* const json_string) {

	return get_normalized_string_from_codepoints(json_string->value);
}

static void json_to_string_string_impl(StringBuilder* const sb, const JsonString* const json_string,
                                       const JsonSerializeOptions options) {
	UNUSED(options);

	// TODO: escape chacraters, that need escaping
	tstr string_unescaped = get_tstr_from_json_string(json_string);

	if(tstr_is_null(&string_unescaped)) {
		OOM_ASSERT(false, "error in formatting json string");
	}

	STRING_BUILDER_APPENDF(sb, OOM_ASSERT(false, "error in formatting json string");
	                       , "\"" TSTR_FMT "\"", TSTR_FMT_ARGS(string_unescaped));

	tstr_free(&string_unescaped);
}

static void json_to_string_variant_impl(StringBuilder* const sb, const JsonValue json_value,
                                        const JsonSerializeOptions options);

#define FORMAT_TSTR(tstr_res, statement, format, ...) \
	do { \
		char* buf = NULL; \
		FORMAT_STRING(&buf, statement, format, __VA_ARGS__); \
		tstr_res = tstr_own_cstr(buf); \
	} while(false)

static void json_to_string_array_impl(StringBuilder* const sb, const JsonArray* const json_array,
                                      const JsonSerializeOptions options) {

	tstr start_str = TSTR_LIT("[");
	tstr separator_str = TSTR_LIT(", ");
	tstr end_str = TSTR_LIT("]");

	if(options.indent_size != 0) {
		if(options.indent_size > 4) {
			start_str = TSTR_LIT("[\n");
			separator_str = TSTR_LIT(",\n");
			end_str = TSTR_LIT("\n]");
		} else {
			FORMAT_TSTR(start_str, OOM_ASSERT(false, "error in formatting json array");
			            , "[%*s", (int)options.indent_size, "");

			FORMAT_TSTR(separator_str, OOM_ASSERT(false, "error in formatting json array");
			            , ",%*s", (int)options.indent_size, "");

			FORMAT_TSTR(end_str, OOM_ASSERT(false, "error in formatting json array");
			            , "%*s]", (int)options.indent_size, "");
		}
	}

	string_builder_append_tstr(sb, &start_str);

	for(size_t i = 0; i < json_array_size(json_array); ++i) {
		if(i != 0) {
			string_builder_append_tstr(sb, &separator_str);
		}

		const JsonValue value = json_array_at(json_array, i);
		json_to_string_variant_impl(sb, value, options);
	}

	string_builder_append_tstr(sb, &end_str);

	tstr_free(&start_str);
	tstr_free(&separator_str);
	tstr_free(&end_str);
}

static void json_to_string_object_impl(StringBuilder* const sb, const JsonObject* const json_object,
                                       const JsonSerializeOptions options) {
	tstr start_str = TSTR_LIT("{");
	tstr separator_str = TSTR_LIT(", ");
	tstr end_str = TSTR_LIT("}");

	if(options.indent_size != 0) {
		if(options.indent_size > 4) {
			start_str = TSTR_LIT("{\n");
			separator_str = TSTR_LIT(",\n");
			end_str = TSTR_LIT("\n}");
		} else {
			FORMAT_TSTR(start_str, OOM_ASSERT(false, "error in formatting json object");
			            , "{%*s", (int)options.indent_size, "");

			FORMAT_TSTR(separator_str, OOM_ASSERT(false, "error in formatting json object");
			            , ",%*s", (int)options.indent_size, "");

			FORMAT_TSTR(end_str, OOM_ASSERT(false, "error in formatting json object");
			            , "%*s}", (int)options.indent_size, "");
		}
	}

	string_builder_append_tstr(sb, &start_str);

	JsonObjectIter* iter = json_object_get_iterator(json_object);

	bool start = true;

	while(true) {

		const JsonObjectEntry* const next_entry = json_object_iterator_next(iter);

		if(next_entry == NULL) {
			break;
		}

		if(!start) {
			string_builder_append_tstr(sb, &separator_str);
		} else {
			start = false;
		}

		const JsonString* const key = json_object_entry_get_key(next_entry);

		json_to_string_string_impl(sb, key, options);
		string_builder_append_tstr_static(sb, TSTR_STATIC_LIT(": "));

		const JsonValue value = json_object_entry_get_value(next_entry);

		json_to_string_variant_impl(sb, value, options);
	}

	string_builder_append_tstr(sb, &end_str);

	tstr_free(&start_str);
	tstr_free(&separator_str);
	tstr_free(&end_str);
}

static void json_to_string_variant_impl(StringBuilder* const sb, const JsonValue json_value,
                                        const JsonSerializeOptions options) {
	SWITCH_JSON_VALUE(json_value) {
		CASE_JSON_VALUE_IS_OBJECT_CONST(json_value) {
			json_to_string_object_impl(sb, object.obj, options);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_ARRAY_CONST(json_value) {
			json_to_string_array_impl(sb, array.arr, options);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_NUMBER_CONST(json_value) {
			json_to_string_number_impl(sb, number, options);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_STRING_CONST(json_value) {
			json_to_string_string_impl(sb, string, options);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_BOOLEAN_CONST(json_value) {
			json_to_string_boolean_impl(sb, boolean, options);
		}
		break;
		VARIANT_CASE_END();
		CASE_JSON_VALUE_IS_NULL() {
			json_to_string_null_impl(sb, options);
		}
		break;
		VARIANT_CASE_END();
		default: {
			break;
		}
	}
}

NODISCARD tstr json_value_to_string_advanced(const JsonValue json_value,
                                             const JsonSerializeOptions options) {

	StringBuilder* sb = string_builder_init();

	json_to_string_variant_impl(sb, json_value, options);

	const SizedBuffer buffer = string_builder_release_into_sized_buffer(&sb);

	return tstr_own(buffer.data, buffer.size, buffer.size);
}

NODISCARD bool json_string_eq(const JsonString* const str1, const JsonString* const str2) {
	const size_t len1 = TVEC_LENGTH(Utf8Codepoint, str1->value);
	const size_t len2 = TVEC_LENGTH(Utf8Codepoint, str2->value);

	if(len1 != len2) {
		return false;
	}

	const Utf8Codepoint* const data1 = TVEC_DATA_CONST(Utf8Codepoint, &(str1->value));
	const Utf8Codepoint* const data2 = TVEC_DATA_CONST(Utf8Codepoint, &(str2->value));

	return memcmp(data1, data2, sizeof(*data1) * len1) == 0;
}

NODISCARD size_t json_array_size(const JsonArray* const array) {
	return TVEC_LENGTH(JsonValue, array->value);
}

NODISCARD JsonValue json_array_at(const JsonArray* const array, const size_t index) {
	assert(index < json_array_size(array));

	return TVEC_AT(JsonValue, array->value, index);
}

NODISCARD size_t json_object_count(const JsonObject* const object) {
	return TMAP_SIZE(JsonValueMapImpl, &(object->value));
}

struct JsonObjectEntryImpl {
	TMAP_TYPENAME_ENTRY(JsonValueMapImpl) value;
};

NODISCARD const JsonObjectEntry* json_object_get_entry_by_key(const JsonObject* const object,
                                                              const JsonString* const key) {

	if(key == NULL) {
		return NULL;
	}

	const JsonObjectKey correct_key = { .string = *key };

	const TMAP_TYPENAME_ENTRY(JsonValueMapImpl)* const result =
	    TMAP_GET_ENTRY(JsonValueMapImpl, &(object->value), correct_key);

	// as this struct has only one member, it is safe to cast this
	return (const JsonObjectEntry*)result;
}

struct JsonObjectIterImpl {
	TMAP_TYPENAME_ITER(JsonValueMapImpl) iter;
	JsonObjectEntry value_slot;
};

NODISCARD JsonObjectIter* json_object_get_iterator(const JsonObject* const object) {

	JsonObjectIter* iter = malloc(sizeof(JsonObjectIter));

	if(iter == NULL) {
		return NULL;
	}

	iter->iter = TMAP_ITER_INIT(JsonValueMapImpl, &(object->value));
	iter->value_slot = ZERO_STRUCT(JsonObjectEntry);

	return iter;
}

NODISCARD const JsonObjectEntry* json_object_iterator_next(JsonObjectIter* const iter) {

	const bool result = TMAP_ITER_NEXT(JsonValueMapImpl, &(iter->iter), &(iter->value_slot.value));

	if(!result) {
		return NULL;
	}

	return &(iter->value_slot);
}

void json_object_free_iterator(JsonObjectIter* const iter) {
	free(iter);
}

NODISCARD const JsonString* json_object_entry_get_key(const JsonObjectEntry* const object_entry) {
	const TMAP_TYPENAME_ENTRY(JsonValueMapImpl)* const entry =
	    (const TMAP_TYPENAME_ENTRY(JsonValueMapImpl)*)object_entry;

	return &(entry->key.string);
}

NODISCARD JsonValue json_object_entry_get_value(const JsonObjectEntry* const object_entry) {
	const TMAP_TYPENAME_ENTRY(JsonValueMapImpl)* const entry =
	    (const TMAP_TYPENAME_ENTRY(JsonValueMapImpl)*)object_entry;

	return entry->value;
}

NODISCARD JsonString* json_get_string_from_cstr(const char* cstr) {
	return json_get_string_from_tstr_view(tstr_view_from(cstr));
}

NODISCARD JsonString* json_get_string_from_tstr(const tstr* str) {
	return json_get_string_from_tstr_view(tstr_as_view(str));
}

NODISCARD JsonString* json_get_string_from_tstr_view(tstr_view str_view) {
	JsonString* string = get_empty_json_string_impl();

	if(string == NULL) {
		return NULL;
	}

	const Utf8DataResult result = get_utf8_string(str_view.data, str_view.len);

#define FREE_AT_END() \
	do { \
		free_json_string(string); \
	} while(false)

	IF_UTF8_DATA_RESULT_IS_ERROR_IGN(result) {
		FREE_AT_END();
		return NULL;
	}

	const Utf8Data data = utf8_data_result_get_as_ok(result).result;

	for(size_t i = 0; i < data.size; ++i) {
		const Utf8Codepoint codepoint = data.data[i];

		const tstr_static add_result = json_string_add_char_impl(string, codepoint);

		if(!tstr_static_is_null(add_result)) {
			FREE_AT_END();
			return NULL;
		}
	}

	free_utf8_data(data);

	return string;
}
#undef FREE_AT_END

static void json_format_source_location_impl(StringBuilder* const sb,
                                             const SourceLocation location) {

	if(is_null_source_location(location)) {
		string_builder_append_single(sb, "<Nowhere>");
		return;
	}

	SWITCH_JSON_SOURCE(location.source) {
		CASE_JSON_SOURCE_IS_FILE_CONST(location.source) {
			STRING_BUILDER_APPENDF(sb, return;, TSTR_FMT ":%zu:%zu", TSTR_FMT_ARGS(*file.file_path),
			                                  location.pos.line, location.pos.col)
			return;
		}
		VARIANT_CASE_END();
		CASE_JSON_SOURCE_IS_STRING_IGN() {
			// TODO: print the whole line of this string and after that the error
			STRING_BUILDER_APPENDF(sb, return;
			                       , "<string source>:%zu:%zu", location.pos.line, location.pos.col)
			return;
		}
		VARIANT_CASE_END();
		default: {
			return;
		}
	}
}

NODISCARD tstr json_format_source_location(const SourceLocation location) {
	StringBuilder* sb = string_builder_init();

	json_format_source_location_impl(sb, location);

	const SizedBuffer buffer = string_builder_release_into_sized_buffer(&sb);

	return tstr_own(buffer.data, buffer.size, buffer.size);
}

NODISCARD tstr json_format_error(const JsonError error) {

	StringBuilder* sb = string_builder_init();

	string_builder_append_tstr_static(sb, error.message);

	string_builder_append_single(sb, ": ");

	json_format_source_location_impl(sb, error.loc);

	const SizedBuffer buffer = string_builder_release_into_sized_buffer(&sb);

	return tstr_own(buffer.data, buffer.size, buffer.size);
}
