

#include "./state.h"

#include <stdlib.h>
#include <string.h>

// see https://datatracker.ietf.org/doc/html/rfc959#section-5
FTPState* alloc_default_state(const char* global_folder) {
	FTPState* state = (FTPState*)malloc(sizeof(FTPState));

	if(!state) {
		return NULL;
	}

	size_t global_folder_length = strlen(global_folder) + 1;

	// invariant check
	if(global_folder[global_folder_length - 2] == '/') {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 1 violated\n");
		return NULL;
	}

	state->current_working_directory = (char*)malloc(global_folder_length);

	if(!state->current_working_directory) {
		free(state);
		return NULL;
	}

	memcpy(state->current_working_directory, global_folder, global_folder_length);

	AccountInfo* account = alloc_default_account();

	if(!account) {
		return NULL;
	}

	state->account = account;

	state->global_folder = global_folder;
	state->current_type = FTP_TRANSMISSION_TYPE_ASCII | FTP_TRANSMISSION_TYPE_FLAG_NP;
	state->mode = FTP_MODE_STREAM;
	state->structure = FTP_STRUCTURE_FILE;

	return state;
}

// note: global_folder <-> current_working_directory invariants:
// both end NOT in /
// cwd is the same or a subpath of global_f

char* get_current_dir_name(FTPState* state, bool escape) {
	const char* global_folder = state->global_folder;

	char* current_working_directory = state->current_working_directory;

	size_t g_length = strlen(global_folder);
	size_t c_length = strlen(current_working_directory);

	// invariant check 1
	if((global_folder[g_length - 1] == '/') || (current_working_directory[c_length - 1] == '/')) {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 1 violated\n");
		return NULL;
	}

	// invariant check 2
	if(strstr(current_working_directory, global_folder) != current_working_directory) {
		LOG_MESSAGE_SIMPLE(LogLevelCritical | LogPrintLocation, "folder invariant 2 violated\n");
		return NULL;
	}

	// escape " aS ""
	if(escape) {

		size_t needed_size = c_length == g_length ? 1 : (c_length - g_length) * 2;

		char* result = malloc(needed_size + 1);

		if(!result) {
			return NULL;
		}

		if(c_length == g_length) {
			memcpy(result, "/", needed_size);

			result[needed_size] = '\0';
		} else {
			size_t pos = 0;
			size_t result_pos = 0;
			while(true) {
				if(current_working_directory[pos] == '\0') {
					result[result_pos] = '\0';
					break;
				} else if(current_working_directory[pos] == '"') {
					result[result_pos] = '"';
					result[result_pos + 1] = '"';
					result_pos += 2;
				} else {
					result[result_pos] = current_working_directory[pos];
					result_pos++;
				}
				pos++;
			}
		}

		return result;
	}

	size_t needed_size = c_length == g_length ? 1 : c_length - g_length;

	char* result = malloc(needed_size + 1);

	if(!result) {
		return NULL;
	}

	if(c_length == g_length) {
		memcpy(result, "/", needed_size);
	} else {
		memcpy(result, current_working_directory + g_length, needed_size);
	}

	result[needed_size] = '\0';

	return result;
}
