

#pragma once

#include "./routes.h"
#include "utils/clock.h"
#include "utils/utils.h"

#include <tvec.h>

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ServeFolderResultTypeNotFound = 0,
	ServeFolderResultTypeServerError,
	ServeFolderResultTypeFile,
	ServeFolderResultTypeFolder
} ServeFolderResultType;

typedef struct {
	tstr mime_type;
	SizedBuffer file_content;
	tstr file_name;
} ServeFolderFileInfo;

// NOTe. similar to some ftp type, but with less info
typedef struct {
	bool dir;
	char* file_name;
	Time date;
	size_t size;
} ServeFolderFolderEntry;

TVEC_DEFINE_VEC_TYPE(ServeFolderFolderEntry)

typedef struct {
	TVEC_TYPENAME(ServeFolderFolderEntry) entries;
	bool has_valid_parent;
} ServeFolderFolderInfo;

typedef struct {
	ServeFolderResultType type;
	union {
		ServeFolderFileInfo file;
		ServeFolderFolderInfo folder;
	} data;
} ServeFolderResult;

NODISCARD ServeFolderResult* get_serve_folder_content(HttpRequestProperties http_properties,
                                                      HTTPRouteServeFolder data,
                                                      HTTPSelectedRoute selected_route_data,
                                                      bool send_body);

void free_serve_folder_result(ServeFolderResult* serve_folder_result);

NODISCARD StringBuilder* folder_content_to_html(ServeFolderFolderInfo folder_info,
                                                const tstr* folder_path);
