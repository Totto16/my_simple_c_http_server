

#include "./file_ops.h"
#include "generic/send.h"
#include "utils/clock.h"
#include "utils/path.h"
#include "utils/string_builder.h"

#include <cwalk.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
// this is a way by hdining the real struct dataementation withour using opaque pointers
typedef struct {
	size_t total_count;
	size_t sent_count;
	// TODO(Totto): support records, so  that we can keep track of the records we sent!
} SendProgressData;

struct SendProgressImpl {
	bool finished;
	SendProgressData data;
};

// TODO(Totto): use more cwk_* functions from cwalk, see https://likle.github.io/cwalk/reference/

// note: global_folder <-> current_working_directory invariants:
// both end NOT in /
// cwd is the same or a subpath of global_f

char* get_current_dir_name_relative_to_ftp_root(const FTPState* const state, bool escape) {

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
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
		            "folder invariant 1 violated: %s\n", file);
		return NULL;
	}

	// invariant check 2
	if(strstr(file, global_folder) != file) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
		            "folder invariant 2 violated: %s\n", file);
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
			result[0] = '/';

			result[needed_size] = '\0';
		} else {
			size_t pos = g_length;
			size_t result_pos = 0;
			while(true) {
				if(file[pos] == '\0') {
					result[result_pos] = '\0';
					break;
				}

				if(file[pos] == '"') {
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
		result[0] = '/';
	} else {
		memcpy(result, file + g_length, needed_size);
	}

	result[needed_size] = '\0';

	return result;
}

NODISCARD static char*
resolve_abs_path_in_cwd(const FTPState* const state, // NOLINT(misc-no-recursion)
                        const char* const user_file_input_to_sanitize, DirChangeResult* result) {

	// normalize the absolute path, so that eg <g>/../ gets resolved, then it fails one invariant
	// below, so it poses no risk to path traversal

	size_t buffer_size = strlen(user_file_input_to_sanitize) + 1;

	char* file = (char*)malloc(buffer_size * sizeof(char));

	if(!file) {
		if(result) {
			*result = DirChangeResultError;
		}

		return NULL;
	}

	size_t buffer_size_result = cwk_path_normalize(user_file_input_to_sanitize, file, buffer_size);

	// the normalization had not enough bytes in the file buffer
	if(buffer_size_result >= buffer_size) {
		if(result) {
			*result = DirChangeResultError;
		}

		return NULL;
	}

	// NOTE: file has to satisfy the same invariants as the current_working_directory
	const char* global_folder = state->global_folder;

	size_t g_length = strlen(global_folder);
	size_t f_length = strlen(file);

	// invariant check 1
	if((global_folder[g_length - 1] == '/') || (file[f_length - 1] == '/')) {
		if(result) {
			*result = DirChangeResultError;
		} else {
			LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
			            "folder invariant 1 violated: %s\n", file);
		}

		return NULL;
	}

	// invariant check 2
	if(strstr(file, global_folder) != file) {
		if(result) {
			*result = DirChangeResultErrorPathTraversal;
		} else {
			LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
			            "folder invariant 2 violated: %s\n", file);
		}

		return NULL;
	}

	// everything is fine, after the invariants are
	if(result) {
		*result = DirChangeResultOk;
	}
	return file;
}

NODISCARD static char* internal_resolve_path_in_cwd(const FTPState* const state,
                                                    const char* const file,
                                                    DirChangeResult* dir_result) {

	// check if the file is absolute or relative
	bool is_absolute = file_is_absolute(file);

	const char* base_dir = is_absolute // NOLINT(readability-implicit-bool-conversion)
	                           ? state->global_folder
	                           : state->current_working_directory;

	size_t b_length = strlen(base_dir);
	size_t f_length = strlen(file);

	// 1 for the "/" (only if relative) and one for the \0 byte
	size_t final_length =
	    b_length + f_length + (is_absolute ? 1 : 2); // NOLINT(readability-implicit-bool-conversion)
	char* abs_string = malloc(final_length * sizeof(char));

	if(!abs_string) {
		return NULL;
	}

	// NOTE: no need to normalize the resulting path to avoid bad patterns (e.g. <g>/../..), that is
	// done in resolve_abs_path_in_cwd
	memcpy(abs_string, base_dir, b_length);

	if(!is_absolute) {
		abs_string[b_length] = '/';
	}
	memcpy(abs_string + b_length +
	           (is_absolute ? 0 : 1), // NOLINT(readability-implicit-bool-conversion)
	       file, f_length);
	abs_string[final_length - 1] = '\0';

	char* result = resolve_abs_path_in_cwd(state, abs_string, dir_result);

	free(abs_string);

	return result;
}

NODISCARD char* resolve_path_in_cwd(const FTPState* const state, const char* const file) {
	return internal_resolve_path_in_cwd(state, file, NULL);
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	SendTypeFile = 0,
	SendTypeMultipleFiles,
	SendTypeRawData,
} SendType;

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
	dev_t dev;
	ino_t ino;
} UniqueIdentifier;

typedef struct {
	mode_t mode;
	size_t link_amount;
	Owners owners;
	size_t size;
	Time last_mod;
	char* file_name;
	UniqueIdentifier identifier;
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
	FileSendFormat format;
} MultipleFiles;

typedef struct {
	void* data;
	size_t size;
} RawData;

typedef struct {
	FileWithMetadata* file;
	FileSendFormat format;
} SingleFile;

struct SendDataImpl {
	SendType type;
	union {
		SingleFile* file;
		MultipleFiles* multiple_files;
		RawData data;
	} value;
};

NODISCARD SendProgress* setup_send_progress(const SendData* const data, SendMode send_mode) {

	SendProgress* progress = malloc(sizeof(SendProgress));

	if(!progress) {
		return NULL;
	}

	progress->finished = false;
	progress->data = (SendProgressData){ .total_count = 0, .sent_count = 0 };

	size_t original_data_count = 0;

	switch(data->type) {
		case SendTypeFile: {
			original_data_count = 1;
			break;
		}
		case SendTypeMultipleFiles: {
			original_data_count = data->value.multiple_files->count;
			break;
		}
		case SendTypeRawData: {
			original_data_count = data->value.data.size;
			break;
		}
		default: break;
	}

	size_t actual_count = 0;

	switch(send_mode) {
		case SendModeStreamBinaryFile: {
			actual_count = original_data_count;
			break;
		}
		case SendModeStreamBinaryRecord: {
			// TODO(Totto): calculate this based on record metadata etc
			break;
		}

		case SendModeUnsupported:
		default: break;
	}

	progress->data.total_count = actual_count;

	return progress;
}

NODISCARD bool send_progress_is_finished(SendProgress* progress) {
	return progress->finished;
}

void free_send_progress(SendProgress* progress) {
	free(progress);
}

NODISCARD static char get_type_from_mode(mode_t mode) {
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

NODISCARD static FilePermissions permissions_from_mode(mode_t mode) {

	char special_type = get_type_from_mode(mode);

	FilePermissions file_permissions = { .special_type = special_type, .permissions = {} };

	int masks[3] = { S_IRWXU, S_IRWXG, S_IRWXO };
	for(size_t i = 0; i < 3; ++i) {
		int mask = masks[i];
		mode_t value =
		    (mode & mask) >> ((2 - i) * 3); // NOLINT(readability-implicit-bool-conversion)
		file_permissions.permissions[i] = (OnePermission){ .read = (value & 0b100) != 0,
			                                               .write = (value & 0b010) != 0,
			                                               .execute = (value & 0b001) != 0 };
	}

	return file_permissions;
}

NODISCARD FileWithMetadata* get_metadata_for_file(const char* absolute_path, const char* name);

NODISCARD static FileWithMetadata* get_metadata_for_file_abs(char* const absolute_path) {

	char* name_ptr = absolute_path;

	// TODO. factor out  into helper function or use cwalk
	while(true) {
		char* result = strstr(name_ptr, "/");

		if(result == NULL) {
			break;
		}

		name_ptr = result + 1;
	}

	char* final_name = strdup(name_ptr);

	FileWithMetadata* result = get_metadata_for_file(absolute_path, final_name);

	free(final_name);

	return result;
}

NODISCARD static SingleFile* get_metadata_for_single_file(char* const absolute_path,
                                                          FileSendFormat format) {

	SingleFile* result = (SingleFile*)malloc(sizeof(SingleFile));

	if(!result) {
		return NULL;
	}

	result->format = format;

	FileWithMetadata* file = get_metadata_for_file_abs(absolute_path);

	if(!file) {
		free(result);
		return NULL;
	}

	result->file = file;

	return result;
}

NODISCARD static FileWithMetadata* get_metadata_for_file_folder(const char* const folder,
                                                                const char* const name) {

	size_t folder_len = strlen(folder);
	size_t name_len = strlen(name);

	bool has_trailing_backslash = folder[folder_len - 1] == '/';
	size_t final_length =
	    folder_len + name_len +
	    (has_trailing_backslash ? 1 : 2); // NOLINT(readability-implicit-bool-conversion)

	char* absolute_path = (char*)malloc(final_length);

	if(!absolute_path) {
		return NULL;
	}

	memcpy(absolute_path, folder, folder_len);

	if(!has_trailing_backslash) {
		absolute_path[folder_len] = '/';
	}

	memcpy(absolute_path + folder_len +
	           (has_trailing_backslash ? 0 : 1), // NOLINT(readability-implicit-bool-conversion)
	       name, name_len);

	absolute_path[final_length - 1] = '\0';

	FileWithMetadata* result = get_metadata_for_file(absolute_path, name);

	free(absolute_path);

	return result;
}

NODISCARD FileWithMetadata* get_metadata_for_file(
    const char* const absolute_path, // NOLINT(bugprone-easily-swappable-parameters)
    const char* const name) {

	FileWithMetadata* metadata = (FileWithMetadata*)malloc(sizeof(FileWithMetadata));

	if(!metadata) {
		return NULL;
	}

	struct stat stat_result;
	int result = stat(absolute_path, &stat_result);

	if(result != 0) {
		LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
		            "Couldn't stat folder '%s': %s\n", absolute_path, strerror(errno));

		free(metadata);
		return NULL;
	}

	Owners owners = { .user = stat_result.st_uid, .group = stat_result.st_gid };

	size_t name_len = strlen(name);

	char* new_name = (char*)malloc(name_len + 1);

	if(!new_name) {
		free(metadata);
		return NULL;
	}

	memcpy(new_name, name, name_len);

	new_name[name_len] = '\0';

	metadata->mode = stat_result.st_mode;
	metadata->link_amount = stat_result.st_nlink;
	metadata->owners = owners;
	metadata->size = stat_result.st_size;
#ifdef __APPLE__
	metadata->last_mod = time_from_struct(stat_result.st_mtimespec);
#else
	metadata->last_mod = time_from_struct(stat_result.st_mtim);
#endif

	metadata->file_name = new_name;
	metadata->identifier.dev = stat_result.st_dev;
	metadata->identifier.ino = stat_result.st_ino;

	return metadata;
}

static void free_file_metadata(FileWithMetadata* metadata) {

	free(metadata->file_name);
	free(metadata);
}

static inline size_t size_for_number(size_t num) {

	return ((size_t)(floor(log10((double)num)))) + 1;
}

NODISCARD static MaxSize internal_get_size_for(FileWithMetadata* metadata) {

	MaxSize result = {};

	result.link = size_for_number(metadata->link_amount);
	result.user = size_for_number(metadata->owners.user);
	result.group = size_for_number(metadata->owners.group);
	result.size = size_for_number(metadata->size);

	return result;
}

static void internal_update_max_size(MaxSize* sizes, FileWithMetadata* metadata) {

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

NODISCARD static MultipleFiles* get_files_in_folder(const char* const folder,
                                                    FileSendFormat format) {

	MultipleFiles* result = (MultipleFiles*)malloc(sizeof(MultipleFiles));

	if(!result) {
		return NULL;
	}

	result->count = 0;
	result->files = NULL;
	result->sizes = (MaxSize){ .link = 0, .user = 0, .group = 0, .size = 0 };
	result->format = format;

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
		}

		if(strcmp("..", name) == 0) {
			continue;
		}

		FileWithMetadata* metadata = get_metadata_for_file_folder(folder, name);

		if(!metadata) {
			goto error;
		}

		result->count++;

		// TODO: we have many places, where raw ararys instead of ZVEc are used, fix that,
		// search for realloc for that!

		FileWithMetadata** new_array = (FileWithMetadata**)realloc(
		    (void*)result->files, sizeof(FileWithMetadata*) * result->count);

		if(!new_array) {
			free(metadata);
			goto error;
		}

		result->files = new_array;

		result->files[result->count - 1] = metadata;
		internal_update_max_size(&result->sizes, metadata);
	}

error:

	if(result->files != NULL) {
		for(size_t i = 0; i < result->count; ++i) {
			free_file_metadata(result->files[i]);
		}
	}
success:

	int close_res = closedir(dir);
	if(close_res != 0) {
		free(result);
		return NULL;
	}

	return result;
}

NODISCARD SendData* get_data_to_send_for_list(bool is_folder, char* const path,
                                              FileSendFormat format) {

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
		data->type = SendTypeMultipleFiles;
		MultipleFiles* files = get_files_in_folder(path, format);
		if(files == NULL) {
			free(data);
			return NULL;
		}
		data->value.multiple_files = files;
	} else {
		data->type = SendTypeFile;
		SingleFile* file = get_metadata_for_single_file(path, format);

		if(file == NULL) {
			free(data);
			return NULL;
		}

		data->value.file = file;
	}

	return data;
}

NODISCARD SendData* get_data_to_send_for_retr(char* path) {

	SendData* data = (SendData*)malloc(sizeof(SendData));

	if(!data) {
		return NULL;
	}

	size_t file_size = 0;

	void* file_data = read_entire_file(path, &file_size);

	if(file_data == NULL) {

		free(data);
		return NULL;
	}

	RawData raw_data = { .data = file_data, .size = file_size };

	data->type = SendTypeRawData;
	data->value.data = raw_data;

	return data;
}

#define FORMAT_SPACES "    "

NODISCARD static StringBuilder* format_file_line_in_ls_format(FileWithMetadata* file,
                                                              MaxSize sizes) {

	StringBuilder* string_builder = string_builder_init();

	if(!string_builder) {
		return NULL;
	}

	// append permissions string

	{
		FilePermissions permissions = permissions_from_mode(file->mode);

		char modes[3][3] = {};

		for(size_t i = 0; i < 3; ++i) {

			OnePermission perm = permissions.permissions[i];

			modes[i][0] = perm.read ? 'r' : '-';    // NOLINT(readability-implicit-bool-conversion)
			modes[i][1] = perm.write ? 'w' : '-';   // NOLINT(readability-implicit-bool-conversion)
			modes[i][2] = perm.execute ? 'x' : '-'; // NOLINT(readability-implicit-bool-conversion)
		}

		STRING_BUILDER_APPENDF(string_builder, return NULL;, "%c%.*s%.*s%.*s",
		                                                   permissions.special_type, 3, modes[0], 3,
		                                                   modes[1], 3, modes[2]);
	}

	char* date_str = get_date_string(file->last_mod, TimeFormatFTP);

	if(date_str == NULL) {
		return NULL;
	}

	STRING_BUILDER_APPENDF(
	    string_builder,
	    {
		    free(date_str);
		    return NULL;
	    },
	    FORMAT_SPACES "%*lu" FORMAT_SPACES "%*d" FORMAT_SPACES "%*d" FORMAT_SPACES "%*lu %s %s\n",
	    ((int)sizes.link), file->link_amount, ((int)sizes.user), file->owners.user,
	    ((int)sizes.group), file->owners.group, ((int)sizes.size), file->size, date_str,
	    file->file_name);

	free(date_str);
	return string_builder;
}

#undef FORMAT_SPACES

#ifdef __APPLE__
	#define DEV_FMT "%d"
	#define INO_FMT "%llu"
#else
	#define DEV_FMT "%lu"
	#define INO_FMT "%lu"
#endif

#define ELPF_PRETTY_PRINT_PERMISSIONS true

NODISCARD static StringBuilder* format_file_line_in_eplf_format(FileWithMetadata* file) {

	StringBuilder* string_builder = string_builder_init();

	if(!string_builder) {
		return NULL;
	}

	// see https://cr.yp.to/ftp/list/eplf.html

	// 1. a plus sign (\053);
	string_builder_append_single(string_builder, "+");

	// 2. a series of facts about the file;
	{

		bool is_dir = S_ISDIR(file->mode);

		if(!is_dir) {
			// can be retrieved
			string_builder_append_single(string_builder, "r,");
		}

		if(is_dir) {
			// i a dir essentially
			string_builder_append_single(string_builder, "/,");
		}

		if(!is_dir) {
			// has a size
			STRING_BUILDER_APPENDF(string_builder, return NULL;, "s%lu,", file->size);
		}

		// last mod time in UNIX epoch seconds
		STRING_BUILDER_APPENDF(string_builder, return NULL;
		                       , "m%lu,", get_time_in_seconds(file->last_mod));

		// unique identifier (dev.ino)
		STRING_BUILDER_APPENDF(string_builder, return NULL;, "i" DEV_FMT "." INO_FMT ",",
		                                                   file->identifier.dev,
		                                                   file->identifier.ino);

		if(ELPF_PRETTY_PRINT_PERMISSIONS) {
			FilePermissions permissions = permissions_from_mode(file->mode);

			char modes[3][3] = {};

			for(size_t i = 0; i < 3; ++i) {

				OnePermission perm = permissions.permissions[i];

				modes[i][0] = perm.read ? 'r' : '-'; // NOLINT(readability-implicit-bool-conversion)
				modes[i][1] =
				    perm.write ? 'w' : '-'; // NOLINT(readability-implicit-bool-conversion)
				modes[i][2] =
				    perm.execute ? 'x' : '-'; // NOLINT(readability-implicit-bool-conversion)
			}

			// permissions, look nicer, many clients just display the string, NOT spec compliant
			STRING_BUILDER_APPENDF(string_builder, return NULL;, "up%c%.*s%.*s%.*s",
			                                                   permissions.special_type, 3,
			                                                   modes[0], 3, modes[1], 3, modes[2]);
		} else {

			uint32_t permission = (S_IRWXU | S_IRWXG | S_IRWXO) & file->mode;

			// permissions, according to spec
			STRING_BUILDER_APPENDF(string_builder, return NULL;, "up%o,", permission);
		}
	}

	// 3. a tab (\011);
	// 4. an abbreviated pathname; and
	// 5. \015\012. (\r\n)
	STRING_BUILDER_APPENDF(string_builder, return NULL;, "\t%s\r\n", file->file_name);

	return string_builder;
}

NODISCARD static StringBuilder* format_file_line(FileWithMetadata* file, MaxSize sizes,
                                                 FileSendFormat format) {

	switch(format) {
		case FileSendFormatLs: {
			return format_file_line_in_ls_format(file, sizes);
		}
		case FileSendFormatEplf: {
			return format_file_line_in_eplf_format(file);
		}
		default: return NULL;
	}
}

#define SEND_CHUNK_SIZE 0xFF

NODISCARD bool send_data_to_send(const SendData* const data, ConnectionDescriptor* descriptor,
                                 SendMode send_mode, SendProgress* progress) {

	if(progress->finished) {
		return true;
	}

	if(progress->data.sent_count >= progress->data.total_count) {
		progress->finished = true;
		return true;
	}

	switch(send_mode) {
		case SendModeStreamBinaryFile: {
			break;
		}
		case SendModeStreamBinaryRecord: {
			return false;
		}

		case SendModeUnsupported:
		default: return false;
	}

	switch(data->type) {
		case SendTypeFile: {
			// TODO(Totto): dataement
			return false;
			break;
		}
		case SendTypeMultipleFiles: {
			FileWithMetadata* value = data->value.multiple_files->files[progress->data.sent_count];

			StringBuilder* string_builder = format_file_line(
			    value, data->value.multiple_files->sizes, data->value.multiple_files->format);

			if(!string_builder) {
				return false;
			}

			int send_result = send_string_builder_to_connection(descriptor, &string_builder);
			if(send_result < 0) {
				return false;
			}

			progress->data.sent_count++;

			break;
		}

		case SendTypeRawData: {
			RawData raw_data = data->value.data;

			size_t offset = progress->data.sent_count;

			void* to_send = ((uint8_t*)raw_data.data) + offset;

			size_t send_length = SEND_CHUNK_SIZE;

			if(offset + send_length >= progress->data.total_count) {
				send_length = progress->data.total_count - offset;
			}

			int send_result = send_data_to_connection(descriptor, to_send, send_length);
			if(send_result < 0) {
				return false;
			}

			progress->data.sent_count += send_length;

			return true;
			break;
		}
		default: break;
	}

	if(progress->data.sent_count >= progress->data.total_count) {
		progress->finished = true;
	}

	return true;
}

void free_send_data(SendData* data) {
	// TODO(Totto): dataement
	UNUSED(data);
}

NODISCARD DirChangeResult change_dirname_to(FTPState* state, const char* file) {

	DirChangeResult dir_result = DirChangeResultOk;
	char* new_dir = internal_resolve_path_in_cwd(state, file, &dir_result);

	if(!new_dir) {
		if(dir_result == DirChangeResultErrorPathTraversal) {
			// change nothing, leave as is
			return DirChangeResultOk;
		}

		return DirChangeResultNoSuchDir;
	}

	// invariant check 1
	if(new_dir[strlen(new_dir) - 1] == '/') {
		return DirChangeResultError;
	}

	// invariant check 2
	if(strstr(new_dir, state->global_folder) != new_dir) {
		return DirChangeResultErrorPathTraversal;
	}

	state->current_working_directory = new_dir;

	return DirChangeResultOk;
}

NODISCARD bool write_to_file(char* path, void* data, size_t data_size) {

	FILE* file = fopen(path, "wb");

	if(file == NULL) {
		LOG_MESSAGE(LogLevelError, "Couldn't open file for writing '%s': %s\n", path,
		            strerror(errno));

		return false;
	}

	size_t fwrite_result = fwrite(data, 1, data_size, file);

	if(fwrite_result != data_size) {
		LOG_MESSAGE(LogLevelError, "Couldn't write the correct amount of bytes to file '%s': %s\n",
		            path, strerror(errno));

		return false;
	}

	int fclose_result = fclose(file);

	if(fclose_result != 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't close file '%s': %s\n", path, strerror(errno));

		return false;
	}

	return true;
}
