#include "./folder.h"
#include "./mime.h"

#include "./debug.h"
#include "utils/clock.h"
#include "utils/path.h"

#include <cwalk.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

TVEC_IMPLEMENT_VEC_TYPE(ServeFolderFolderEntry)

static char* get_final_file_path(HTTPRouteServeFolder data, const char* const route_path,
                                 ParsedURLPath request_path) {

	// Note: check that some of these have trailing slashes, atm i normalize them after adding a
	// slash after each of them, but that is no entirely correct
	// TODO(Totto): redirect /dir requests to /dir/ with 310 or 302!

	size_t data_len = strlen(data.folder_path);
	size_t request_len = tstr_len(&request_path.path);

	char* result_path = NULL;

	/**
	 * see the declaration of @link{HTTPRouteServeFolderType}  for more information on what this
	 * means
	 */
	switch(data.type) {
		case HTTPRouteServeFolderTypeRelative: {
			size_t route_len = strlen(route_path);

			// + 3 = 2 * '/' char and 0 trailing byte!
			size_t final_size = data_len + route_len + request_len + 3;
			result_path = malloc(final_size * sizeof(char));
			if(!result_path) {
				break;
			}

			memcpy(result_path, data.folder_path, data_len);
			result_path[data_len] = '/';

			memcpy(result_path + data_len + 1, route_path, route_len);
			result_path[data_len + 1 + route_len] = '/';

			memcpy(result_path + data_len + 2 + route_len, tstr_cstr(&request_path.path),
			       request_len);
			result_path[data_len + 2 + route_len + request_len] = '\0';

			break;
		}
		case HTTPRouteServeFolderTypeAbsolute: {
			// + 2 = 1 * '/' char and 0 trailing byte!
			size_t final_size = data_len + request_len + 1;
			result_path = malloc(final_size * sizeof(char));
			if(!result_path) {
				break;
			}

			memcpy(result_path, data.folder_path, data_len);
			result_path[data_len] = '/';

			memcpy(result_path + data_len + 1, tstr_cstr(&request_path.path), request_len);
			result_path[data_len + 1 + request_len] = '\0';

			break;
		}
		default: {
			return NULL;
		}
	}

	if(result_path == NULL) {
		return NULL;
	}

	{ // inline normalize result_path

		size_t buffer_size = strlen(result_path) + 1;

		char* buffer = (char*)malloc(buffer_size * sizeof(char));

		if(!buffer) {
			free(result_path);

			return NULL;
		}

		size_t buffer_size_result = cwk_path_normalize(result_path, buffer, buffer_size);

		// the normalization had not enough bytes in the file buffer
		if(buffer_size_result >= buffer_size) {
			free(result_path);
			free(buffer);

			return NULL;
		}

		free(result_path);
		result_path = buffer;
	}

	return result_path;
}

static ServeFolderResult get_serve_folder_content_for_file(const tstr* const path,
                                                           const bool send_body) {

	ServeFolderResult result = { .type = ServeFolderResultTypeServerError };

	tstr_view base_name = tstr_as_view(path);

	{
		tstr_split_iter iter = tstr_split_init(base_name, "/");

		// TODO(Totto): factor out  into helper function or use cwalk
		while(true) {
			tstr_view part;

			bool split_succeeded = tstr_split_next(&iter, &part);

			if(!split_succeeded) {
				break;
			}

			base_name = part;
		}
	}

	tstr_view ext = TSTR_EMPTY_VIEW;

	{

		tstr_split_iter ext_iter = tstr_split_init(base_name, ".");

		while(true) {
			tstr_view part;
			bool split_succeeded = tstr_split_next(&ext_iter, &part);

			if(!split_succeeded) {
				break;
			}

			ext = part;
		}
	}

	tstr mime_type = get_mime_type_for_ext(ext);

	if(tstr_is_null(&mime_type)) {
		mime_type = UNRECOGNIZED_MIME_TYPE;
	}

	tstr file_name = tstr_from_view(base_name);

	if(tstr_is_null(&file_name)) {
		result.type = ServeFolderResultTypeServerError;
		return result;
	}

	size_t file_size = 0;

	SizedBuffer file_content = { .data = NULL, .size = 0 };

	if(!send_body) {
		// we just need the size, no need to read the entire file
		bool success = get_file_size_of_file(tstr_cstr(path), &file_size);

		if(!success) {

			tstr_free(&file_name);
			result.type = ServeFolderResultTypeServerError;
			return result;
		}

		file_content = (SizedBuffer){ .data = NULL, .size = file_size };
	} else {
		void* file_data = read_entire_file(tstr_cstr(path), &file_size);

		if(file_data == NULL) {

			tstr_free(&file_name);
			result.type = ServeFolderResultTypeServerError;
			return result;
		}

		file_content = (SizedBuffer){ .data = file_data, .size = file_size };
	}

	ServeFolderFileInfo file_info = {
		.file_content = file_content,
		.mime_type = mime_type,
		.file_name = file_name,
	};

	result.type = ServeFolderResultTypeFile;
	result.data.file = file_info;

	return result;
}

static void free_folder_info_entry(ServeFolderFolderEntry folder_info_entry) {

	free(folder_info_entry.file_name);
}

static void free_folder_info(ServeFolderFolderInfo folder_info) {

	for(size_t i = 0; i < TVEC_LENGTH(ServeFolderFolderEntry, folder_info.entries); ++i) {

		const ServeFolderFolderEntry entry =
		    TVEC_AT(ServeFolderFolderEntry, folder_info.entries, i);

		free_folder_info_entry(entry);
	}

	TVEC_FREE(ServeFolderFolderEntry, &folder_info.entries);
}

NODISCARD static ServeFolderFolderEntry get_folder_entry_for_file(
    const char* const absolute_path, // NOLINT(bugprone-easily-swappable-parameters)
    const char* const name) {

	ServeFolderFolderEntry result = { .file_name = NULL };

	struct stat stat_result;
	int result_stat_int = stat(absolute_path, &stat_result);

	if(result_stat_int != 0) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
		            "Couldn't stat folder '%s': %s\n", absolute_path, strerror(errno));

		result.file_name = NULL;
		return result;
	}

	size_t name_len = strlen(name);

	char* new_name = (char*)malloc(name_len + 1);

	if(!new_name) {
		result.file_name = NULL;
		return result;
	}

	memcpy(new_name, name, name_len);

	new_name[name_len] = '\0';

	// TODO(totto): should some files be hidden, like e.g. symlinks, block devices etc?

	const bool is_dir = (stat_result.st_mode & S_IFMT) == S_IFDIR;

	result.dir = is_dir;
	result.file_name = new_name;

#ifdef __APPLE__
	result.date = time_from_struct(stat_result.st_mtimespec);
#else
	result.date = time_from_struct(stat_result.st_mtim);
#endif

	result.size = stat_result.st_size;

	return result;
}

NODISCARD static ServeFolderFolderEntry get_folder_entry(const char* const folder,
                                                         const char* const name) {

	ServeFolderFolderEntry result = { .file_name = NULL };

	size_t folder_len = strlen(folder);
	size_t name_len = strlen(name);

	bool has_trailing_backslash = folder[folder_len - 1] == '/';
	size_t final_length =
	    folder_len + name_len +
	    (has_trailing_backslash ? 1 : 2); // NOLINT(readability-implicit-bool-conversion)

	char* absolute_path = (char*)malloc(final_length);

	if(!absolute_path) {
		result.file_name = NULL;
		return result;
	}

	memcpy(absolute_path, folder, folder_len);

	if(!has_trailing_backslash) {
		absolute_path[folder_len] = '/';
	}

	memcpy(absolute_path + folder_len +
	           (has_trailing_backslash ? 0 : 1), // NOLINT(readability-implicit-bool-conversion)
	       name, name_len);

	absolute_path[final_length - 1] = '\0';

	result = get_folder_entry_for_file(absolute_path, name);

	free(absolute_path);

	return result;
}

NODISCARD static ServeFolderResult
get_serve_folder_content_for_folder(const char* const folder_path, bool has_valid_parent) {

	ServeFolderResult result = { .type = ServeFolderResultTypeServerError };

	ServeFolderFolderInfo folder_info = {
		.entries = TVEC_EMPTY(ServeFolderFolderEntry),
		.has_valid_parent = has_valid_parent,
	};

	DIR* dir = opendir(folder_path);
	if(dir == NULL) {
		free_folder_info(folder_info);
		result.type = ServeFolderResultTypeServerError;
		return result;
	}

	while(true) {

		errno = 0;
		struct dirent* ent = readdir(dir);
		if(ent == NULL) {
			if(errno == 0) {
				// success: all files listed
				goto success;
			}

			// an error occurred
			goto error;
		}

		char* name = ent->d_name;

		if(strcmp(".", name) == 0) {
			continue;
		}

		if(strcmp("..", name) == 0) {
			continue;
		}

		ServeFolderFolderEntry metadata = get_folder_entry(folder_path, name);

		if(!metadata.file_name) {
			goto error;
		}

		TvecResult push_res = TVEC_PUSH(ServeFolderFolderEntry, &folder_info.entries, metadata);

		if(push_res != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)

			free_folder_info_entry(metadata);

			goto error;
		}
	}

error:
	free_folder_info(folder_info);

success:

	int close_res = closedir(dir);

	if(close_res != 0) {
		result.type = ServeFolderResultTypeServerError;

		free_folder_info(folder_info);
		return result;
	}

	result.type = ServeFolderResultTypeFolder;
	result.data.folder = folder_info;
	return result;
}

NODISCARD static inline uint64_t abs_i64(int64_t value) {
	if(value < 0) {
		// handles overflow in then - MIN_I64 is bigger than MAX_I64 by one
		return ((uint64_t)(-(value + 1))) + 1;
	}

	return (uint64_t)value;
}

/**
 * @brief Check if both paths are the same
 *        handles "/dir/" == "/dir"
 *        the path need to be at least partially normalized, the only non normalized thing is the
 * end
 *
 * @param path1
 * @param path2
 * @return NODISCARD
 */
NODISCARD static bool is_the_same_path(const char* path1, const char* path2) {

	const size_t len1 = strlen(path1);

	const size_t len2 = strlen(path2);

	if(len1 == len2) {
		return strncmp(path1, path2, len1) == 0;
	}

	uint64_t diff = abs_i64((int64_t)len1) - ((int64_t)len2);

	if(diff > 1) {
		return false;
	}

	if(diff == 1) {

		size_t min = 0;
		const char* has_slash = NULL;
		size_t hash_len = 0;

		if(len1 < len2) {
			min = len1;
			has_slash = path2;
			hash_len = len2;
		} else {
			min = len2;
			has_slash = path1;
			hash_len = len1;
		}

		if(has_slash[hash_len - 1] != '/') {
			return false;
		}

		return strncmp(path1, path2, min) == 0;
	}

	return false;
}

NODISCARD ServeFolderResult* get_serve_folder_content(HttpRequestProperties http_properties,
                                                      HTTPRouteServeFolder data,
                                                      HTTPSelectedRoute selected_route_data,
                                                      const bool send_body) {

	ServeFolderResult* result = malloc(sizeof(ServeFolderResult));

	if(!result) {
		return NULL;
	}

	result->type = ServeFolderResultTypeServerError;

	if(!file_is_absolute(data.folder_path)) {
		result->type = ServeFolderResultTypeServerError;
		return result;
	}

	if(http_properties.type != HTTPPropertyTypeNormal) {
		result->type = ServeFolderResultTypeServerError;
		return result;
	}

	const ParsedURLPath normal_data = http_properties.data.normal;

	char* final_path = get_final_file_path(data, selected_route_data.original_path, normal_data);

	if(final_path == NULL) {
		result->type = ServeFolderResultTypeServerError;
		return result;
	}

	// invariant check, the new result is a subfolder or the same of the serve_directory (prevents
	// ".." path traversal)
	if(strstr(final_path, data.folder_path) != final_path) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
		            "folder invariant violated: %s\n", final_path);

		free(final_path);

		result->type = ServeFolderResultTypeServerError;
		return result;
	}

	struct stat stat_result;
	int stat_result_int = stat(final_path, &stat_result);

	if(stat_result_int != 0) {
		switch(errno) {
			case EACCES:
			case ENOENT: {
				result->type = ServeFolderResultTypeNotFound;
				break;
			}
			default: {
				result->type = ServeFolderResultTypeServerError;
				break;
			}
		}

		free(final_path);

		return result;
	}

	bool is_folder = S_ISDIR(stat_result.st_mode);

	if(is_folder) {

		const bool has_valid_parent =
		    !is_the_same_path( // NOLINT(readability-implicit-bool-conversion)
		        final_path, data.folder_path);

		*result = get_serve_folder_content_for_folder(final_path, has_valid_parent);
	} else {
		const tstr path = tstr_own_cstr(final_path);

		*result = get_serve_folder_content_for_file(&path, send_body);
	}

	free(final_path);

	return result;
}

static void free_file_info(ServeFolderFileInfo file_info) {
	free_sized_buffer(file_info.file_content);
	tstr_free(&file_info.file_name);
}

void free_serve_folder_result(ServeFolderResult* serve_folder_result) {

	if(!serve_folder_result) {
		return;
	}

	switch(serve_folder_result->type) {
		case ServeFolderResultTypeNotFound:
		case ServeFolderResultTypeServerError: {
			break;
		}
		case ServeFolderResultTypeFile: {
			free_file_info(serve_folder_result->data.file);
			break;
		}
		case ServeFolderResultTypeFolder: {
			free_folder_info(serve_folder_result->data.folder);
			break;
		}
		default: {
			break;
		}
	}

	free(serve_folder_result);
}

#ifdef __APPLE__
	#define TIME_FORMAT "%llu"
#else
	#define TIME_FORMAT "%zu"
#endif

NODISCARD static StringBuilder* folder_content_add_entry(StringBuilder* body,
                                                         ServeFolderFolderEntry entry) {

	// TODO(Totto): better format it, add link / a for folder or file, better format date!

	const uint64_t time_in_ms = get_time_in_milli_seconds(entry.date);

	const char* dir_delim = "";

	if(entry.dir) {
		dir_delim = "/";
	}

	STRING_BUILDER_APPENDF(
	    body, return NULL;
	    ,
	    "<div class=\"entry\"> <div class=\"name\"><a href=\"%s%s\">%s%s</a></div> <div "
	    "class=\"date\">" TIME_FORMAT "</div> <div class=\"size\">%zu</div> </div> <br>",
	    entry.file_name, dir_delim, entry.file_name, dir_delim, time_in_ms, entry.size);

	return body;
}

NODISCARD StringBuilder* folder_content_to_html(ServeFolderFolderInfo folder_info,
                                                const tstr* const folder_path) {

	StringBuilder* body = string_builder_init();

	STRING_BUILDER_APPENDF(body, return NULL;
	                       , "<h1 id=\"title\">Index of %s</h1> <br>", tstr_cstr(folder_path));

	string_builder_append_single(body, "<div id=\"content\">");

	if(folder_info.has_valid_parent) {
		char* file_name_dup = strdup("..");

		ServeFolderFolderEntry parent = {
			.file_name = file_name_dup,
			.dir = true,
			.date = empty_time(),
			.size = 0,
		};

		if(!folder_content_add_entry(body, parent)) {

			free_string_builder(body);
			free(file_name_dup);
			return NULL;
		}

		free(file_name_dup);
	}

	for(size_t i = 0; i < TVEC_LENGTH(ServeFolderFolderEntry, folder_info.entries); ++i) {

		ServeFolderFolderEntry entry = TVEC_AT(ServeFolderFolderEntry, folder_info.entries, i);

		if(!folder_content_add_entry(body, entry)) {
			return NULL;
		}
	}

	string_builder_append_single(body, "</div>");

	// style

	StringBuilder* style = string_builder_init();
	string_builder_append_single(
	    style, "body{background: #a6a5a1;}"
	           "\n"
	           "#content {  display: flex; flex-flow: column; align-items: center;}"
	           "\n"
	           ".entry{ width: 100%; background: #666; text-align: center; color: white; display: "
	           "grid; padding: 10px; "
	           "grid-template-columns: 50% 10% 40%; justify-items: start; }"

	);

	// script
	StringBuilder* script = string_builder_init();
	string_builder_append_single(
	    script, "function setDynamicContent(){"
	            "/*TODO: make date and sizes human readable in the Intl format of the user! */"
	            "window.addEventListener('DOMContentLoaded',setDynamicContent);");

	StringBuilder* html_result = html_from_string(NULL, script, style, body);
	return html_result;
}
