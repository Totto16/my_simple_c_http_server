#pragma once

#include <stdbool.h>
#include <utils/utils.h>

#include "variants.h"

#ifdef __cplusplus
extern "C" {
#endif

// see: https://datatracker.ietf.org/doc/html/rfc8259

typedef struct JsonObjectImpl JsonObject;

typedef struct JsonArrayImpl JsonArray;

typedef struct JsonStringImpl JsonString;

typedef struct {
	double value;
} JsonNumber;

typedef struct {
	bool value;
} JsonBoolean;

typedef struct {
	size_t line;
	size_t col;
} SourcePosition;

typedef struct {
	// NOTE: this "references" the original file path data, it can be freed, as this is passed by
	// the user, the user can only free the file tstr, that he passed in, if he doesn't use this
	// anymore
	const tstr* file_path;
} JsonFileSource;

typedef struct {
	// NOTE: this "references" the original data, it can be freed, as this is passed by the user,
	// the user can only free the underlying data of the tstr_view, that he passed in, if he doesn't
	// use this anymore
	tstr_view data;
} JsonStringSource;

GENERATE_VARIANT_ALL_JSON_SOURCE()

typedef struct {
	JsonSource source;
	SourcePosition pos;
} SourceLocation;

typedef struct {
	tstr_static message;
	// note: this has references to the original passed in data, either a tstr file_path or  the
	// tstr_view of the passed ins tring, so it is only valid, until the user freed that data
	SourceLocation loc;
} JsonError;

GENERATE_VARIANT_ALL_JSON_VARIANT()

GENERATE_VARIANT_ALL_JSON_PARSE_RESULT()

// parse json strings

NODISCARD JsonParseResult json_variant_parse_from_str(tstr_view data);

NODISCARD JsonParseResult json_variant_parse_from_file(const tstr* file_path);

void free_json_variant(JsonVariant* json_variant);

// serialize json values

typedef struct {
	size_t indent_size;
} JsonSerializeOptions;

NODISCARD tstr json_variant_to_string(JsonVariant json_variant);

NODISCARD tstr json_variant_to_string_advanced(JsonVariant json_variant,
                                               JsonSerializeOptions options);

// utility / get functions

NODISCARD bool json_string_eq(const JsonString* str1, const JsonString* str2);

NODISCARD size_t json_array_size(const JsonArray* array);

NODISCARD JsonVariant json_array_at(const JsonArray* array, size_t index);

NODISCARD size_t json_object_count(const JsonObject* object);

typedef struct JsonObjectEntryImpl JsonObjectEntry;

NODISCARD const JsonObjectEntry* json_object_get_entry_by_key(const JsonObject* object,
                                                              const JsonString* key);

typedef struct JsonObjectIterImpl JsonObjectIter;

NODISCARD JsonObjectIter* json_object_get_iterator(const JsonObject* object);

NODISCARD const JsonObjectEntry* json_object_iterator_next(JsonObjectIter* iter);

void json_object_free_iterator(JsonObjectIter* iter);

NODISCARD const JsonString* json_object_entry_get_key(const JsonObjectEntry* object_entry);

NODISCARD JsonVariant json_object_entry_get_value(const JsonObjectEntry* object_entry);

// create functions

NODISCARD JsonString* json_get_string_from_cstr(const char* cstr);

NODISCARD JsonString* json_get_string_from_tstr(const tstr* str);

NODISCARD JsonString* json_get_string_from_tstr_view(tstr_view str_view);

void free_json_string(JsonString* json_string);

NODISCARD JsonArray* get_empty_json_array(void);

NODISCARD tstr_static json_array_add_entry(JsonArray* json_array, JsonVariant entry);

void free_json_array(JsonArray* json_arr);

NODISCARD JsonObject* get_empty_json_object(void);

NODISCARD tstr_static json_object_add_entry(JsonObject* json_object, JsonString** key_moved,
                                            JsonVariant value);

NODISCARD tstr_static json_object_add_entry_tstr(JsonObject* json_object, const tstr* key,
                                                 JsonVariant value);

NODISCARD tstr_static json_object_add_entry_cstr(JsonObject* json_object, const char* key,
                                                 JsonVariant value);

// utility functions

NODISCARD SourceLocation make_null_source_location(void);

NODISCARD bool is_null_source_location(SourceLocation location);

void free_json_object(JsonObject* json_obj);

#ifdef __cplusplus
}
#endif
