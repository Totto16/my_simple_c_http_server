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

typedef struct {
	double value;
} JsonNumber;

typedef struct {
	tstr value;
} JsonString;

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

#ifdef __cplusplus
}
#endif
