

#include "./file_ops.h"
#include "generic/send.h"
#include "utils/string_builder.h"

#include <dirent.h>
#include <math.h>
#include <sys/stat.h>
#include <time.h>

// note: global_folder <-> current_working_directory invariants:
// both end NOT in /
// cwd is the same or a subpath of global_f

char* get_current_dir_name(const FTPState* const state, bool escape) {

	char* current_working_directory = state->current_working_directory;

	return get_dir_name_relative_to_ftp_root(state, current_working_directory, escape);
}

char* get_dir_name_relative_to_ftp_root(const FTPState* const state, const char* const file,
                                        bool escape) {
	// NOTE: file has to satisfy the same invariants as the current_working_directory
	const char* global_folder = state->global_folder;

	size_t g_length = strlen(global_folder);
	size_t f_length = strlen(file);

	// invariant check 1
	if((global_folder[g_length - 1] == '/') || (file[f_length - 1] == '/')) {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 1 violated\n");
		return NULL;
	}

	// invariant check 2
	if(strstr(file, global_folder) != file) {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 2 violated\n");
		return NULL;
	}

	// escape " as ""
	if(escape) {

		size_t needed_size = f_length == g_length ? 1 : (f_length - g_length) * 2;

		char* result = malloc(needed_size + 1);

		if(!result) {
			return NULL;
		}

		if(f_length == g_length) {
			memcpy(result, "/", needed_size);

			result[needed_size] = '\0';
		} else {
			size_t pos = 0;
			size_t result_pos = 0;
			while(true) {
				if(file[pos] == '\0') {
					result[result_pos] = '\0';
					break;
				} else if(file[pos] == '"') {
					result[result_pos] = '"';
					result[result_pos + 1] = '"';
					result_pos += 2;
				} else {
					result[result_pos] = file[pos];
					result_pos++;
				}
				pos++;
			}
		}

		return result;
	}

	size_t needed_size = f_length == g_length ? 1 : f_length - g_length;

	char* result = malloc(needed_size + 1);

	if(!result) {
		return NULL;
	}

	if(f_length == g_length) {
		memcpy(result, "/", needed_size);
	} else {
		memcpy(result, file + g_length, needed_size);
	}

	result[needed_size] = '\0';

	return result;
}

bool file_is_absolute(const char* const file) {
	if(strlen(file) == 0) {
		// NOTE: 0 length is the same as "." so it is not absolute
		return false;
	}

	return file[0] == '/';
}

NODISCARD char* resolve_abs_path_in_cwd(const FTPState* const state,
                                        const char* const user_File_input_to_sanitize) {

	// normalize the absolute path, so that eg <g>/../ gets resolved, then it fails one invariant
	// below, so it poses no risk to path traversal
	char* file = realpath(user_File_input_to_sanitize, NULL);

	if(!file) {
		return NULL;
	}

	// NOTE: file has to satisfy the same invariants as the current_working_directory
	const char* global_folder = state->global_folder;

	size_t g_length = strlen(global_folder);
	size_t f_length = strlen(file);

	// invariant check 1
	if((global_folder[g_length - 1] == '/') || (file[f_length - 1] == '/')) {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 1 violated\n");
		return NULL;
	}

	// invariant check 2
	if(strstr(file, global_folder) != file) {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 2 violated\n");
		return NULL;
	}

	// everything is fine, after the invariants are checked
	return file;
}

NODISCARD char* resolve_path_in_cwd(const FTPState* const state, const char* const file) {

	// check if the file is absolute or relative
	if(file_is_absolute(file)) {
		return resolve_abs_path_in_cwd(state, file);
	}

	char* current_working_directory = state->current_working_directory;

	size_t c_length = strlen(current_working_directory);
	size_t f_length = strlen(file);

	// 1 for the "/"" and one for the \0 byte
	char* abs_string = malloc(c_length + f_length + 2);

	if(!abs_string) {
		return NULL;
	}

	// NOTE: no need to normalize the resulting path to avoid bad patterns (e.g. <g>/../..), that is
	// done in resolve_abs_path_in_cwd
	memcpy(abs_string, current_working_directory, c_length);
	abs_string[c_length] = '/';
	memcpy(abs_string + c_length + 1, file, f_length);
	abs_string[c_length + f_length + 1] = '\0';

	char* result = resolve_abs_path_in_cwd(state, abs_string);

	free(abs_string);

	return result;
}

/**
 * @enum value
 */
typedef enum { SEND_TYPE_FILE = 0, SEND_TYPE_MULTIPLE_FILES, SEND_TYPE_RAW_DATA } SendType;

typedef struct {
	bool read;
	bool write;
	bool execute;
} OnePermission;

typedef struct {
	char special_type;
	OnePermission permissions[3];
} FilePermissions;

typedef struct {
	uid_t user;  /* User ID of owner */
	gid_t group; /* Group ID of owner */
} Owners;

typedef struct {
	FilePermissions permissions;
	size_t link_amount;
	Owners owners;
	size_t size;
	time_t last_mod;
	char* file_name;
} FileWithMetadata;

typedef struct {
	size_t link;
	size_t user;
	size_t group;
	size_t size;
} MaxSize;

typedef struct {
	FileWithMetadata** files;
	size_t count;
	MaxSize sizes;
} MultipleFiles;

typedef struct {
	void* data;
	size_t size;
} RawData;

struct SendDataImpl {
	SendType type;
	union {
		FileWithMetadata* file;
		MultipleFiles* multiple_files;
		RawData data;
	} data;
};

NODISCARD SendProgress setup_send_progress(const SendData* const data, SendMode send_mode) {

	SendProgress result = { .finished = false, ._impl = { .total_count = 0, .sent_count = 0 } };

	size_t original_data_count = 0;

	switch(data->type) {
		case SEND_TYPE_FILE: {
			original_data_count = 1;
			break;
		}
		case SEND_TYPE_MULTIPLE_FILES: {
			original_data_count = data->data.multiple_files->count;
			break;
		}
		case SEND_TYPE_RAW_DATA: {
			original_data_count = data->data.data.size;
			break;
		}
		default: break;
	}

	size_t actual_count = 0;

	switch(send_mode) {
		case SEND_MODE_STREAM_BINARY_FILE: {
			actual_count = original_data_count;
			break;
		}
		case SEND_MODE_STREAM_BINARY_RECORD: {
			// TODO calculate this based on record metadata etc
			break;
		}

		case SEND_MODE_UNSUPPORTED:
		default: break;
	}

	result._impl.total_count = actual_count;

	return result;
}

NODISCARD char get_type_from_mode(mode_t mode) {
	switch(mode & S_IFMT) {
		case S_IFREG: return '-';
		case S_IFBLK: return 'b';
		case S_IFCHR: return 'c';
		case S_IFDIR: return 'd';
		case S_IFIFO: return 'p';
		case S_IFLNK: return 'l';
		case S_IFSOCK: return 's';
		default: return '?';
	}
}

NODISCARD FilePermissions permissions_from_mode(mode_t mode) {

	char special_type = get_type_from_mode(mode);

	FilePermissions file_permissions = { .special_type = special_type, .permissions = {} };

	int masks[3] = { S_IRWXU, S_IRWXG, S_IRWXO };
	for(size_t i = 0; i < 3; ++i) {
		int mask = masks[i];
		int value = (mode & mask) >> ((2 - i) * 3);
		file_permissions.permissions[i] = (OnePermission){ .read = (value & 0b100) != 0,
			                                               .write = (value & 0b010) != 0,
			                                               .execute = (value & 0b001) != 0 };
	}

	return file_permissions;
}

NODISCARD FileWithMetadata* get_metadata_for_file(const char* absolute_path, const char* name);

NODISCARD FileWithMetadata* get_metadata_for_file_abs(char* const absolute_path) {

	char* name_ptr = absolute_path;

	while(true) {
		char* result = strstr(name_ptr, "/");

		if(result == NULL) {
			break;
		}

		name_ptr = result + 1;
	}

	char* final_name = copy_cstr(name_ptr);

	FileWithMetadata* result = get_metadata_for_file(absolute_path, final_name);

	free(final_name);

	return result;
}

NODISCARD FileWithMetadata* get_metadata_for_file_folder(const char* const folder,
                                                         const char* const name) {

	size_t folder_len = strlen(folder);
	size_t name_len = strlen(name);

	bool has_trailing_backslash = folder[folder_len - 1] == '/';
	size_t final_length = folder_len + name_len + (has_trailing_backslash ? 1 : 2);

	char* absolute_path = (char*)malloc(final_length);

	if(!absolute_path) {
		return NULL;
	}

	memcpy(absolute_path, folder, folder_len);

	if(!has_trailing_backslash) {
		absolute_path[folder_len] = '/';
	}

	memcpy(absolute_path + folder_len + (has_trailing_backslash ? 0 : 1), name, name_len);

	absolute_path[final_length - 1] = '\0';

	FileWithMetadata* result = get_metadata_for_file(absolute_path, name);

	free(absolute_path);

	return result;
}

NODISCARD FileWithMetadata* get_metadata_for_file(const char* const absolute_path,
                                                  const char* const name) {

	FileWithMetadata* metadata = (FileWithMetadata*)malloc(sizeof(FileWithMetadata));

	if(!metadata) {
		return NULL;
	}

	struct stat stat_result;
	int result = stat(absolute_path, &stat_result);

	if(result != 0) {
		LOG_MESSAGE(LogLevelError | LogPrintLocation, "Couldn't stat folder '%s': %s\n",
		            absolute_path, strerror(errno));
		return NULL;
	}

	FilePermissions permissions = permissions_from_mode(stat_result.st_mode);

	Owners owners = { .user = stat_result.st_uid, .group = stat_result.st_gid };

	size_t name_len = strlen(name);

	char* new_name = (char*)malloc(name_len + 1);

	if(!new_name) {
		return NULL;
	}

	memcpy(new_name, name, name_len);

	new_name[name_len] = '\0';

	metadata->permissions = permissions;
	metadata->link_amount = stat_result.st_nlink;
	metadata->owners = owners;
	metadata->size = stat_result.st_size;
	metadata->last_mod = stat_result.st_mtim.tv_sec;
	metadata->file_name = new_name;

	return metadata;
}

void free_file_metadata(FileWithMetadata* metadata) {

	free(metadata->file_name);
	free(metadata);
}

inline size_t size_for_number(size_t num) {

	return floor(log10((double)num)) + 1;
}

NODISCARD MaxSize internal_get_size_for(FileWithMetadata* metadata) {

	MaxSize result = {};

	result.link = size_for_number(metadata->link_amount);
	result.user = size_for_number(metadata->owners.user);
	result.group = size_for_number(metadata->owners.group);
	result.size = size_for_number(metadata->size);

	return result;
}

void internal_update_max_size(MaxSize* sizes, FileWithMetadata* metadata) {

	MaxSize size = internal_get_size_for(metadata);

	if(size.link > sizes->link) {
		sizes->link = size.link;
	}

	if(size.user > sizes->user) {
		sizes->user = size.user;
	}

	if(size.group > sizes->group) {
		sizes->group = size.group;
	}

	if(size.size > sizes->size) {
		sizes->size = size.size;
	}
}

MultipleFiles* get_files_in_folder(const char* const folder) {

	MultipleFiles* result = (MultipleFiles*)malloc(sizeof(MultipleFiles));

	if(!result) {
		return NULL;
	}

	result->count = 0;
	result->files = NULL;
	result->sizes = (MaxSize){ .link = 0, .user = 0, .group = 0, .size = 0 };

	DIR* dir = opendir(folder);
	if(dir == NULL) {
		free(result);
		return NULL;
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
		} else if(strcmp("..", name) == 0) {
			continue;
		}

		FileWithMetadata* metadata = get_metadata_for_file_folder(folder, name);

		if(!metadata) {
			goto error;
		}

		result->count++;

		FileWithMetadata** new_array =
		    (FileWithMetadata**)realloc(result->files, sizeof(FileWithMetadata*) * result->count);

		if(!new_array) {
			goto error;
		}

		result->files = new_array;

		result->files[result->count - 1] = metadata;
		internal_update_max_size(&result->sizes, metadata);
	}

error:
	for(size_t i = 0; i < result->count; ++i) {
		free_file_metadata(result->files[i]);
	}
success:

	int close_res = closedir(dir);
	if(close_res != 0) {
		return NULL;
	}

	return result;
}

NODISCARD SendData* get_data_to_send_for_list(bool is_folder, char* const path) {

	SendData* data = (SendData*)malloc(sizeof(SendData));

	if(!data) {
		return NULL;
	}

	// note: the exact data is not specified by the RFC :(
	// So i just do the stuff that the FTP server did, I used to observer behaviour and looked at
	// filezilla sourcecode, on which format it understands (as it has to parse it)
	// a good in depth explanation is in the filezilla source code at
	// src/engine/directorylistingparser.h:4

	if(is_folder) {
		data->type = SEND_TYPE_MULTIPLE_FILES;
		MultipleFiles* files = get_files_in_folder(path);
		if(files == NULL) {
			free(data);
			return NULL;
		}
		data->data.multiple_files = files;
	} else {
		data->type = SEND_TYPE_FILE;
		FileWithMetadata* file = get_metadata_for_file_abs(path);

		if(file == NULL) {
			free(data);
			return NULL;
		}

		data->data.file = file;
	}

	return data;
}

#define FORMAT_SPACES "    "

NODISCARD StringBuilder* format_file_line(FileWithMetadata* file, MaxSize sizes) {

	StringBuilder* sb = string_builder_init();

	if(!sb) {
		return NULL;
	}

	// append permissions string

	{
		FilePermissions permissions = file->permissions;

		char modes[3][3] = {};

		for(size_t i = 0; i < 3; ++i) {

			OnePermission perm = permissions.permissions[i];

			modes[i][0] = perm.read ? 'r' : '-';
			modes[i][1] = perm.write ? 'w' : '-';
			modes[i][2] = perm.execute ? 'x' : '-';
		}

		string_builder_append(sb, return NULL;, "%c%.*s%.*s%.*s", permissions.special_type, 3,
		                                      modes[0], 3, modes[1], 3, modes[2]);
	}

	size_t max_bytes = 0xFF;
	char* date_str = (char*)malloc(max_bytes * sizeof(char));

	struct tm converted_time;

	struct tm* convert_result = localtime_r(&file->last_mod, &converted_time);

	if(!convert_result) {
		return NULL;
	}

	// see filezilla source code at src/engine/directorylistingparser.cpp:1094 at
	// CDirectoryListingParser::ParseUnixDateTime on why this exact format is used, it has the most
	// available information, while being recognized

	size_t result = strftime(date_str, max_bytes, "%Y-%m-%d %H:%M", &converted_time);

	if(result == 0) {
		free(date_str);
		return NULL;
	}

	date_str[result] = '\0';

	string_builder_append(sb, return NULL;,
	                                      FORMAT_SPACES "%*lu" FORMAT_SPACES "%*d" FORMAT_SPACES
	                                                    "%*d" FORMAT_SPACES "%*lu %s %s\n",
	                                      ((int)sizes.link), file->link_amount, ((int)sizes.user),
	                                      file->owners.user, ((int)sizes.group), file->owners.group,
	                                      ((int)sizes.size), file->size, date_str, file->file_name);

	return sb;
}

#undef FORMAT_SPACES

NODISCARD bool send_data_to_send(const SendData* const data, ConnectionDescriptor* descriptor,
                                 SendMode send_mode, SendProgress* progress) {

	if(progress->finished) {
		return true;
	}

	if(progress->_impl.sent_count >= progress->_impl.total_count) {
		progress->finished = true;
		return true;
	}

	switch(send_mode) {
		case SEND_MODE_STREAM_BINARY_FILE: {
			break;
		}
		case SEND_MODE_STREAM_BINARY_RECORD: {
			return false;
		}

		case SEND_MODE_UNSUPPORTED:
		default: return false;
	}

	switch(data->type) {
		case SEND_TYPE_FILE: {
			// TODO
			return false;
			break;
		}
		case SEND_TYPE_MULTIPLE_FILES: {
			FileWithMetadata* value = data->data.multiple_files->files[progress->_impl.sent_count];

			StringBuilder* string_builder =
			    format_file_line(value, data->data.multiple_files->sizes);

			if(!string_builder) {
				return false;
			}

			int sent_result = sendStringBuilderToConnection(descriptor, string_builder);
			if(sent_result < 0) {
				return false;
			}

			progress->_impl.sent_count++;

			break;
		}

		case SEND_TYPE_RAW_DATA: {
			// TODO
			return false;
			break;
		}
		default: break;
	}

	if(progress->_impl.sent_count >= progress->_impl.total_count) {
		progress->finished = true;
	}

	return true;
}

void free_send_data(SendData* data) {
	// TODO
	UNUSED(data);
}
