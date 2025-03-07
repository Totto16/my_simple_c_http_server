

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

	state->current_working_directory = (char*)malloc(global_folder_length);

	if(!state->current_working_directory) {
		free(state);
		return NULL;
	}

	memcpy(state->current_working_directory, global_folder, global_folder_length);

	state->global_folder = global_folder;
	state->account = NULL;
	state->current_type = FTP_TRANSMISSION_TYPE_ASCII_NP;
	state->mode = FTP_MODE_STREAM;
	state->structure = FTP_STRUCTURE_FILE;

	return state;
}
