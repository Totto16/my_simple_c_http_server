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

GENERATE_VARIANT_ALL_JSON_VARIANT()

GENERATE_VARIANT_ALL_JSON_PARSE_RESULT()

NODISCARD JsonParseResult json_variant_parse_from_str(tstr_view str);

NODISCARD JsonParseResult json_variant_parse_from_file(tstr str);

void free_json_variant(JsonVariant* json_variant);

typedef struct {
	size_t indent_size;
} JsonSerializeOptions;

NODISCARD tstr json_variant_to_string(JsonVariant json_variant);

NODISCARD tstr json_variant_to_string_advanced(JsonVariant json_variant,
                                               JsonSerializeOptions options);

NODISCARD bool json_string_eq(const JsonString* str1, const JsonString* str2);

NODISCARD size_t json_array_size(const JsonArray* array);

NODISCARD JsonVariant json_array_at(const JsonArray* array, size_t index);

NODISCARD size_t json_object_count(const JsonObject* object);

typedef struct JsonObjectEntryImpl JsonObjectEntry;

NODISCARD JsonObjectEntry* json_object_get_entry_by_key(const JsonObject* object, const JsonString* key);

typedef struct JsonObjectIterImpl JsonObjectIter;

NODISCARD JsonObjectIter* json_object_get_iterator(const JsonObject* object);

NODISCARD const JsonObjectEntry* json_object_iterator_next(JsonObjectIter* iter);

void json_object_free_iterator(JsonObjectIter* iter);

NODISCARD const JsonString* json_object_entry_get_key(const JsonObjectEntry* object_entry);

NODISCARD JsonVariant json_object_entry_get_value(const JsonObjectEntry* object_entry);

#ifdef __cplusplus
}
#endif
