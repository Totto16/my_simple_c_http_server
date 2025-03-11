

#include "./file_ops.h"
#include "generic/send.h"

#include <dirent.h>

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
typedef enum {
	SEND_TYPE_FILE_NAME = 0,
	SEND_TYPE_MULTIPLE_FILE_NAMES,
	SEND_TYPE_METADATA,
	SEND_TYPE_RAW_DATA
} SendType;

typedef struct {
	char** files;
	size_t count;
} MultipleFiles;

// TODO:
typedef struct {
	int todo;
} MetaData;

typedef struct {
	void* data;
	size_t size;
} RawData;

struct SendDataImpl {
	SendType type;
	union {
		char* file_name;
		MultipleFiles file_names;
		MetaData metadata;
		RawData data;
	} data;
};

NODISCARD SendProgress setup_send_progress(const SendData* const data, SendMode send_mode) {

	SendProgress result = { .finished = false, ._impl = { .total_count = 0, .sent_count = 0 } };

	size_t original_data_count = 0;

	switch(data->type) {
		case SEND_TYPE_FILE_NAME: {
			original_data_count = 1;
			break;
		}
		case SEND_TYPE_MULTIPLE_FILE_NAMES: {
			original_data_count = data->data.file_names.count;
			break;
		}
		case SEND_TYPE_METADATA: {
			// TODO
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

MetaData get_metadata_for_file(const char* const file) {

	// TODO
	UNUSED(file);
	MetaData metadata = { .todo = 0 };
	return metadata;
}

MultipleFiles get_files_in_folder(const char* const folder) {

	errno = 0;
	MultipleFiles result = { .files = NULL, .count = 0 };

	DIR* dir = opendir(folder);
	if(dir == NULL) {
		if(errno == 0) {
			errno = ENOENT;
		}
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
		size_t name_len = strlen(name);

		char* new_name = (char*)malloc(name_len + 1);

		if(!new_name) {
			errno = ENOMEM;
			goto error;
		}

		result.count++;

		char** new_array = (char**)realloc(result.files, sizeof(char*) * result.count);

		if(!new_array) {
			errno = ENOMEM;
			goto error;
		}

		result.files = new_array;

		result.files[result.count - 1] = new_name;
	}

error:
	for(size_t i = 0; i < result.count; ++i) {
		free(result.files[i]);
	}
success:

	int close_res = closedir(dir);
	if(close_res != 0) {
		if(errno == 0) {
			errno = ENOENT;
		}
	}

	return result;
}

NODISCARD SendData* get_data_to_send_for_list(bool is_folder, const char* const path) {

	SendData* data = (SendData*)malloc(sizeof(SendData));

	if(!data) {
		return NULL;
	}

	// note: the exact data is not specified by the RFC :(
	// So i just do the stuff that the FTP server did, I used to observer behaviour

	if(is_folder) {
		data->type = SEND_TYPE_MULTIPLE_FILE_NAMES;
		MultipleFiles files = get_files_in_folder(path);
		if(errno != 0) {
			free(data);
			return NULL;
		}
		data->data.file_names = files;
	} else {
		data->type = SEND_TYPE_METADATA;
		data->data.metadata = get_metadata_for_file(path);
	}

	return data;
}

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
		case SEND_TYPE_FILE_NAME: {
			// TODO
			return false;
			break;
		}
		case SEND_TYPE_MULTIPLE_FILE_NAMES: {
			char* value = data->data.file_names.files[progress->_impl.sent_count];

			int sent_result = sendStringToConnection(descriptor, value);
			if(sent_result < 0) {
				return false;
			}

			progress->_impl.sent_count++;

			break;
		}
		case SEND_TYPE_METADATA: {
			// TODO
			return false;
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
