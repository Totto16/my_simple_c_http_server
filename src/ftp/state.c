

#include "./state.h"
#include "utils/log.h"

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

NODISCARD static FTPSupportedFeatures* alloc_supported_features(void) {

	FTPSupportedFeatures* supported_features =
	    (FTPSupportedFeatures*)malloc(sizeof(FTPSupportedFeatures));

	if(!supported_features) {
		return NULL;
	}

	supported_features->features = NULL, supported_features->size = 0;

	return supported_features;
}

CustomFTPOptions* alloc_default_options() {
	CustomFTPOptions* options = (CustomFTPOptions*)malloc(sizeof(CustomFTPOptions));

	if(!options) {
		return NULL;
	}

	options->send_format = FILE_SEND_FORMAT_EPLF;

	return options;
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

		free(state);
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
		free(state);
		return NULL;
	}

	state->account = account;

	FTPDataSettings* data_settings = alloc_default_data_settings();

	if(!data_settings) {
		free(state);
		return NULL;
	}

	state->data_settings = data_settings;

	FTPSupportedFeatures* supported_features = alloc_supported_features();

	if(!supported_features) {
		free(state);
		return NULL;
	}

	state->supported_features = supported_features;

	CustomFTPOptions* options = alloc_default_options();

	if(!options) {
		free(state);
		return NULL;
	}

	state->options = options;

	state->global_folder = global_folder;
	state->current_type =
	    FTP_TRANSMISSION_TYPE_ASCII | // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	    FTP_TRANSMISSION_TYPE_FLAG_NP;
	state->mode = FTP_MODE_STREAM;
	state->structure = FTP_STRUCTURE_FILE;

	return state;
}

char* make_address_port_desc(FTPConnectAddr addr) {

	// Format (h1,h2,h3,h4,p1,p2)

	FTPPortField port = addr.port;

	uint8_t port1 =
	    port >> 8; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t port2 =
	    port & 0xFF; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	FTPAddrField address = addr.addr;

	uint8_t host1 =
	    (address >> 24); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t host2 =
	    (address >> 16) & // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	    0xFF;             // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t host3 =
	    (address >> 8) & // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	    0xFF;            // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t host4 =
	    address & 0xFF; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	char* result = NULL;
	FORMAT_STRING(&result, return NULL;
	             , "(%d,%d,%d,%d,%d,%d)", host1, host2, host3, host4, port1, port2);

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
