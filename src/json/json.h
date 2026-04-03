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

NODISCARD JsonParseResult json_variant_parse_from_file(tstr_view str);

void free_json_variant(JsonVariant* json_variant);

#ifdef __cplusplus
}
#endif
