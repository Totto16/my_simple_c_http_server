

#include "./state.h"

#include <stdlib.h>
#include <string.h>

FTPDataSettings* alloc_default_data_settings() {
	FTPDataSettings* data_settings = (FTPDataSettings*)malloc(sizeof(FTPDataSettings));

	if(!data_settings) {
		return NULL;
	}

	data_settings->mode = FTP_DATA_MODE_NONE;
	// ignore: data_settings->addr;

	return data_settings;
}

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

	FTPDataSettings* data_settings = alloc_default_data_settings();

	if(!data_settings) {
		return NULL;
	}

	state->data_settings = data_settings;

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

char* make_address_port_desc(FTPConnectAddr addr) {

	// Format (h1,h2,h3,h4,p1,p2)

	uint16_t port = addr.port;

	uint8_t p1 = port >> 8;
	uint8_t p2 = port & 0xFF;

	uint32_t address = addr.addr;

	uint8_t h1 = (address >> 24);
	uint8_t h2 = (address >> 16) & 0xFF;
	uint8_t h3 = (address >> 8) & 0xFF;
	uint8_t h4 = address & 0xFF;

	char* result = NULL;
	formatString(&result, return NULL;, "(%d,%d,%d,%d,%d,%d)", h1, h2, h3, h4, p1, p2);

	return result;
}

NODISCARD FTPPortInformation get_port_info_from_sockaddr(struct sockaddr_in addr) {

	FTPPortInformation info = { .addr = ntohl(addr.sin_addr.s_addr), .port = ntohs(addr.sin_port) };

	return info;
}
