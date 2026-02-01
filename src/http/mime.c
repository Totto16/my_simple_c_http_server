#define HTTP_MIME_TYPE_IMPL

#include "./mime.h"

ZMAP_IMPLEMENT_MAP_TYPE(char*, CHAR_PTR_KEYNAME, char*, MimeTypeEntryHashMap)

MimeTypeMappings g_mime_type_mappings = {
	.entries = ZMAP_EMPTY_MAP(MimeTypeEntryHashMap),
};

#define ZMAP_INSERT_AND_ASSERT(Typename, map, key, value) \
	do { \
		const ZmapInsertResult insert_result = ZMAP_INSERT(Typename, map, key, value, false); \
		assert(insert_result == ZmapInsertResultOk && "insertion failed"); \
	} while(false)

#define ZMAP_INSERT_AND_ASSERT_MIME_ENTRY(key, value) \
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup(key), \
	                       strdup(value))

static void initialize_mime_type_mappings(void) {

	// html
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("html", "text/html");
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("htm", "text/html");
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("shtml", "text/html");

	// css
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("css", "text/css");

	// xml
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("xml", "text/xml");

	// gif
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("gif", "image/gif");

	// jpeg
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("jpeg", "image/jpeg");
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("jpg", "image/jpeg");

	// js
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("js", "application/javascript");

	// txt
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("txt", "text/plain");

	// svg
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("svg", "image/svg+xml");
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("svgz", "image/svg+xml");

	// woff
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("woff", "font/woff");

	// woff2
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("woff2", "font/woff2");

	// json
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("json", "application/json");

	// pdf
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("pdf", "application/pdf");

	// xhtml
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("xhtml", "application/xhtml+xml");

	// zip
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("zip", "application/zip");

	// mp3
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("mp3", "audio/mpeg");

	// ogg
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("ogg", "audio/ogg");

	// m4a
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("m4a", "audio/x-m4a");

	// mp4
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("mp4", "video/mp4");

	// mpeg
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("mpeg", "video/mpeg");
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("mpg", "video/mpg");

	// webm
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("webm", "video/webm");

	// avi
	ZMAP_INSERT_AND_ASSERT_MIME_ENTRY("avi", "video/x-msvideo");
}

void global_initialize_mime_map(void) {
	initialize_mime_type_mappings();
}

void global_free_mime_map(void) {
	size_t hm_total_length = ZMAP_CAPACITY(g_mime_type_mappings.entries);

	for(size_t i = 0; i < hm_total_length; ++i) {
		const ZMAP_TYPENAME_ENTRY(MimeTypeEntryHashMap)* hm_entry =
		    ZMAP_GET_ENTRY_AT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, i);

		if(hm_entry != NULL && hm_entry != ZMAP_NO_ELEMENT_HERE) {
			free(hm_entry->value);
			free(hm_entry->key);
		}
	}

	ZMAP_FREE(MimeTypeEntryHashMap, &g_mime_type_mappings.entries);
}

NODISCARD const char* get_mime_type_for_ext(const char* ext) {

	if(ZMAP_IS_EMPTY(g_mime_type_mappings.entries)) {
		initialize_mime_type_mappings();
	}

	char* const* result = ZMAP_GET(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, ext);

	if(result == NULL) {
		return UNRECOGNIZED_MIME_TYPE;
	}

	return *result;
}
