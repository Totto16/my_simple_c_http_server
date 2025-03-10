

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

NODISCARD SendMode get_current_send_mode(FTPState* state) {

	// NOTE: state->current_type is a value with flags, so == and != doesn#t work always

	if(state->current_type != FTP_TRANSMISSION_TYPE_IMAGE) {
		return SEND_MODE_UNSUPPORTED;
	}

	if(state->mode != FTP_MODE_STREAM) {
		return SEND_MODE_UNSUPPORTED;
	}

	switch(state->structure) {
		case FTP_STRUCTURE_FILE: {
			return SEND_MODE_STREAM_BINARY_FILE;
		}
		case FTP_STRUCTURE_RECORD: {
			return SEND_MODE_STREAM_BINARY_RECORD;
		}
		case FTP_STRUCTURE_PAGE:
		default: {
			return SEND_MODE_UNSUPPORTED;
		}
	}

	UNREACHABLE();
	return SEND_MODE_UNSUPPORTED;
}
