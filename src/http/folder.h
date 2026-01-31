

#pragma once

#include "./routes.h"
#include "utils/utils.h"
#include <zvec/zvec.h>

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
	const char* mime_type;
	SizedBuffer file_content;
	char* file_name;
} ServeFolderFileInfo;

// NOTe. similar to some ftp type, but with less info
typedef struct {
	bool dir;
	char* file_name;
	struct timespec date;
	size_t size;
} ServeFolderFolderEntry;

ZVEC_DEFINE_VEC_TYPE(ServeFolderFolderEntry)

typedef struct {
	ZVEC_TYPENAME(ServeFolderFolderEntry) entries;
} ServeFolderFolderInfo;

typedef struct {
	ServeFolderResultType type;
	union {
		ServeFolderFileInfo file;
		ServeFolderFolderInfo folder;
	} data;
} ServeFolderResult;

NODISCARD ServeFolderResult* get_serve_folder_content(const HttpRequest* http_request_generic,
                                                      HTTPRouteServeFolder data,
                                                      HTTPSelectedRoute selected_route_data);

void free_serve_folder_result(ServeFolderResult* serve_folder_result);

NODISCARD StringBuilder* folder_content_to_html(ServeFolderFolderInfo folder);
