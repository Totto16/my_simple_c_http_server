#define HTTP_MIME_TYPE_IMPL

#include "./mime.h"

TMAP_IMPLEMENT_MAP_TYPE(char*, CHAR_PTR_KEYNAME, char*, MimeTypeEntryHashMap)

MimeTypeMappings g_mime_type_mappings = {
	.entries = TMAP_EMPTY_MAP(MimeTypeEntryHashMap),
};

#define TMAP_INSERT_AND_ASSERT(Typename, map, key, value) \
	do { \
		const TmapInsertResult insert_result = TMAP_INSERT(Typename, map, key, value, false); \
		assert(insert_result == TmapInsertResultOk && "insertion failed"); \
	} while(false)

#define TMAP_INSERT_AND_ASSERT_MIME_ENTRY(key, value) \
	TMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup(key), \
	                       strdup(value))

static void initialize_mime_type_mappings(void) {

	// html
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("html", "text/html");
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("htm", "text/html");
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("shtml", "text/html");

	// css
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("css", "text/css");

	// xml
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("xml", "text/xml");

	// gif
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("gif", "image/gif");

	// jpeg
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("jpeg", "image/jpeg");
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("jpg", "image/jpeg");

	// js
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("js", "application/javascript");

	// txt
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("txt", "text/plain");

	// svg
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("svg", "image/svg+xml");
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("svgz", "image/svg+xml");

	// woff
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("woff", "font/woff");

	// woff2
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("woff2", "font/woff2");

	// json
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("json", "application/json");

	// pdf
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("pdf", "application/pdf");

	// xhtml
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("xhtml", "application/xhtml+xml");

	// zip
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("zip", "application/zip");

	// mp3
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("mp3", "audio/mpeg");

	// ogg
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("ogg", "audio/ogg");

	// m4a
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("m4a", "audio/x-m4a");

	// mp4
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("mp4", "video/mp4");

	// mpeg
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("mpeg", "video/mpeg");
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("mpg", "video/mpg");

	// webm
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("webm", "video/webm");

	// avi
	TMAP_INSERT_AND_ASSERT_MIME_ENTRY("avi", "video/x-msvideo");
}

void global_initialize_mime_map(void) {
	initialize_mime_type_mappings();
}

void global_free_mime_map(void) {
	size_t hm_total_length = TMAP_CAPACITY(g_mime_type_mappings.entries);

	for(size_t i = 0; i < hm_total_length; ++i) {
		const TMAP_TYPENAME_ENTRY(MimeTypeEntryHashMap)* hm_entry =
		    TMAP_GET_ENTRY_AT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, i);

		if(hm_entry != NULL && hm_entry != TMAP_NO_ELEMENT_HERE) {
			free(hm_entry->value);
			free(hm_entry->key);
		}
	}

	TMAP_FREE(MimeTypeEntryHashMap, &g_mime_type_mappings.entries);
}

NODISCARD const char* get_mime_type_for_ext(const char* ext) {

	if(TMAP_IS_EMPTY(g_mime_type_mappings.entries)) {
		initialize_mime_type_mappings();
	}

	char* const* result = TMAP_GET(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, ext);

	if(result == NULL) {
		return UNRECOGNIZED_MIME_TYPE;
	}

	return *result;
}
