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

static void initialize_mime_type_mappings(void) {

	// html
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("html"),
	                       strdup("text/html"));
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("htm"),
	                       strdup("text/html"));
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("shtml"),
	                       strdup("text/html"));

	// css
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("css"),
	                       strdup("text/css"));

	// xml
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("xml"),
	                       strdup("text/xml"));

	// gif
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("gif"),
	                       strdup("image/gif"));

	// jpeg
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("jpeg"),
	                       strdup("image/jpeg"));
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("jpg"),
	                       strdup("image/jpeg"));

	// js
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("js"),
	                       strdup("application/javascript"));

	// txt
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("txt"),
	                       strdup("text/plain"));

	// svg
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("svg"),
	                       strdup("image/svg+xml"));
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("svgz"),
	                       strdup("image/svg+xml"));

	// woff
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("woff"),
	                       strdup("font/woff"));

	// woff2
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("woff2"),
	                       strdup("font/woff2"));

	// json
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("json"),
	                       strdup("application/json"));

	// pdf
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("pdf"),
	                       strdup("application/pdf"));

	// xhtml
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("xhtml"),
	                       strdup("application/xhtml+xml"));

	// zip
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("zip"),
	                       strdup("application/zip"));

	// mp3
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("mp3"),
	                       strdup("audio/mpeg"));

	// ogg
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("ogg"),
	                       strdup("audio/ogg"));

	// m4a
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("m4a"),
	                       strdup("audio/x-m4a"));

	// mp4
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("mp4"),
	                       strdup("video/mp4"));

	// mpeg
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("mpeg"),
	                       strdup("video/mpeg"));
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("mpg"),
	                       strdup("video/mpg"));

	// webm
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("webm"),
	                       strdup("video/webm"));

	// avi
	ZMAP_INSERT_AND_ASSERT(MimeTypeEntryHashMap, &g_mime_type_mappings.entries, strdup("avi"),
	                       strdup("video/x-msvideo"));
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
