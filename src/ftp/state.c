

#include "./state.h"
#include "utils/log.h"

#include <stdlib.h>
#include <string.h>

static FTPDataSettings* alloc_default_data_settings() {
	FTPDataSettings* data_settings = (FTPDataSettings*)malloc(sizeof(FTPDataSettings));

	if(!data_settings) {
		return NULL;
	}

	data_settings->mode = FtpDataModeNone;
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

static CustomFTPOptions* alloc_default_options() {
	CustomFTPOptions* options = (CustomFTPOptions*)malloc(sizeof(CustomFTPOptions));

	if(!options) {
		return NULL;
	}

	options->send_format = FileSendFormatEplf;

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
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
		                   "folder invariant 1 violated\n");

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
	    FtpTransmissionTypeAscii | // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	    FtpTransmissionTypeFlagNp;
	state->mode = FtpModeStream;
	state->structure = FtpStructureFile;

	return state;
}

char* make_address_port_desc(FTPConnectAddr addr) {

	// Format (h1,h2,h3,h4,p1,p2)

	FTPPortField port = addr.port;

	uint8_t port1 =
	    port >> 8; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	uint8_t port2 =
	    port & 0xFF; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	// TODO(Totto): add explicitly named host or network byte order to the functions and check if it
	// is correct

	IPV4RawBytes raw_ipv4_bytes = get_raw_bytes_as_host_bytes_from_ipv4_address(addr.addr);

	char* result = NULL;
	FORMAT_STRING(&result, return NULL;, "(%d,%d,%d,%d,%d,%d)", raw_ipv4_bytes.bytes[0],
	                                   raw_ipv4_bytes.bytes[1], raw_ipv4_bytes.bytes[2],
	                                   raw_ipv4_bytes.bytes[3], port1, port2);

	return result;
}

NODISCARD FTPPortInformation get_port_info_from_sockaddr(struct sockaddr_in addr) {

	FTPPortInformation info = { .addr = (IPV4Address){ .underlying = addr.sin_addr },
		                        .port = ntohs(addr.sin_port) };

	return info;
}

NODISCARD SendMode get_current_send_mode(FTPState* state) {

	// NOTE: state->current_type is a value with flags, so == and != doesn#t work always

	if(state->current_type != FtpTransmissionTypeImage) {
		return SendModeUnsupported;
	}

	if(state->mode != FtpModeStream) {
		return SendModeUnsupported;
	}

	switch(state->structure) {
		case FtpStructureFile: {
			return SendModeStreamBinaryFile;
		}
		case FtpStructureRecord: {
			return SendModeStreamBinaryRecord;
		}
		case FtpStructurePage:
		default: {
			return SendModeUnsupported;
		}
	}
}
