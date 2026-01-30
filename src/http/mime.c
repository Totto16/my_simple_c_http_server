#define HTTP_MIME_TYPE_IMPL

#include "./mime.h"

MimeTypeMappings g_mime_type_mappings = {
	.entries = STBDS_HASH_MAP_EMPTY,
};

static void initialize_mime_type_mappings(void) {

	// TODO: improve, and also don't allow overwrites, as this is an error in the static mapping

	// html
	stbds_shput(g_mime_type_mappings.entries, strdup("html"), strdup("text/html"));
	stbds_shput(g_mime_type_mappings.entries, strdup("htm"), strdup("text/html"));
	stbds_shput(g_mime_type_mappings.entries, strdup("shtml"), strdup("text/html"));

	// css
	stbds_shput(g_mime_type_mappings.entries, strdup("css"), strdup("text/css"));

	// xml
	stbds_shput(g_mime_type_mappings.entries, strdup("xml"), strdup("text/xml"));

	// gif
	stbds_shput(g_mime_type_mappings.entries, strdup("gif"), strdup("image/gif"));

	// jpeg
	stbds_shput(g_mime_type_mappings.entries, strdup("jpeg"), strdup("image/jpeg"));
	stbds_shput(g_mime_type_mappings.entries, strdup("jpg"), strdup("image/jpeg"));

	// js
	stbds_shput(g_mime_type_mappings.entries, strdup("js"), strdup("application/javascript"));

	// txt
	stbds_shput(g_mime_type_mappings.entries, strdup("txt"), strdup("text/plain"));

	// svg
	stbds_shput(g_mime_type_mappings.entries, strdup("svg"), strdup("image/svg+xml"));
	stbds_shput(g_mime_type_mappings.entries, strdup("svgz"), strdup("image/svg+xml"));

	// woff
	stbds_shput(g_mime_type_mappings.entries, strdup("woff"), strdup("font/woff"));

	// woff2
	stbds_shput(g_mime_type_mappings.entries, strdup("woff2"), strdup("font/woff2"));

	// json
	stbds_shput(g_mime_type_mappings.entries, strdup("json"), strdup("application/json"));

	// pdf
	stbds_shput(g_mime_type_mappings.entries, strdup("pdf"), strdup("application/pdf"));

	// xhtml
	stbds_shput(g_mime_type_mappings.entries, strdup("xhtml"), strdup("application/xhtml+xml"));

	// zip
	stbds_shput(g_mime_type_mappings.entries, strdup("zip"), strdup("application/zip"));

	// mp3
	stbds_shput(g_mime_type_mappings.entries, strdup("mp3"), strdup("audio/mpeg"));

	// ogg
	stbds_shput(g_mime_type_mappings.entries, strdup("ogg"), strdup("audio/ogg"));

	// m4a
	stbds_shput(g_mime_type_mappings.entries, strdup("m4a"), strdup("audio/x-m4a"));

	// mp4
	stbds_shput(g_mime_type_mappings.entries, strdup("mp4"), strdup("video/mp4"));

	// mpeg
	stbds_shput(g_mime_type_mappings.entries, strdup("mpeg"), strdup("video/mpeg"));
	stbds_shput(g_mime_type_mappings.entries, strdup("mpeg"), strdup("video/mpg"));

	// webm
	stbds_shput(g_mime_type_mappings.entries, strdup("webm"), strdup("video/webm"));

	// avi
	stbds_shput(g_mime_type_mappings.entries, strdup("avi"), strdup("video/x-msvideo"));
}

NODISCARD const char* get_mime_type_for_ext(const char* ext) {

	if(g_mime_type_mappings.entries == STBDS_ARRAY_EMPTY) {
		initialize_mime_type_mappings();
	}

	int index = stbds_shgeti(g_mime_type_mappings.entries, ext);

	if(index < 0) {
		return UNRECOGNIZED_MIME_TYPE;
	}

	return g_mime_type_mappings.entries[index].value;
}
