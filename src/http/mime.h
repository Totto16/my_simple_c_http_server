
#pragma once

#include "utils/utils.h"
#include <stb/ds.h>

#define HTTP_MIME_TYPE_NAME(name) g_mime_type_##name

#ifdef HTTP_MIME_TYPE_IMPL
#define HTTP_MIME_TYPE_MAKE(name, content) const char* const HTTP_MIME_TYPE_NAME(name) = content
#else
#define HTTP_MIME_TYPE_MAKE(name, content) extern const char* const HTTP_MIME_TYPE_NAME(name)
#endif

// some Default Mime Type Definitions:

HTTP_MIME_TYPE_MAKE(html, "text/html");

HTTP_MIME_TYPE_MAKE(json, "application/json");

HTTP_MIME_TYPE_MAKE(text, "text/plain");

HTTP_MIME_TYPE_MAKE(octet_stream, "application/octet-stream");

#define DEFAULT_MIME_TYPE MIME_TYPE_HTML

#define MIME_TYPE_HTML HTTP_MIME_TYPE_NAME(html)
#define MIME_TYPE_JSON HTTP_MIME_TYPE_NAME(json)
#define MIME_TYPE_TEXT HTTP_MIME_TYPE_NAME(text)
#define MIME_TYPE_OCTET_STREAM HTTP_MIME_TYPE_NAME(octet_stream)

#define UNRECOGNIZED_MIME_TYPE MIME_TYPE_OCTET_STREAM

// the full list mapped from extension to mime type

// from: https://www.iana.org/assignments/media-types/media-types.xhtml
// and nginx

typedef struct {
	char* key;
	char* value;
} SimpleAccountEntry;

typedef struct {
	STBDS_HASH_MAP(SimpleAccountEntry) entries;
} MimeTypeMappings;

extern MimeTypeMappings g_mime_type_mappings;

NODISCARD const char* get_mime_type_for_ext(const char* ext);
