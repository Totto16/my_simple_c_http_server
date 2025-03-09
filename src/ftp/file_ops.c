

#include "./file_ops.h"

// note: global_folder <-> current_working_directory invariants:
// both end NOT in /
// cwd is the same or a subpath of global_f

char* get_current_dir_name(FTPState* state, bool escape) {

	char* current_working_directory = state->current_working_directory;

	return get_dir_name_relative_to_ftp_root(state, current_working_directory, escape);
}

char* get_dir_name_relative_to_ftp_root(FTPState* state, char* const file, bool escape) {
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

NODISCARD char* resolve_abs_path_in_cwd(FTPState* state, char* const user_File_input_to_sanitize) {

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

NODISCARD char* resolve_path_in_cwd(FTPState* state, char* const file) {

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
