

#include "./state.h"
#include "generic/serialize.h"
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
FTPState* alloc_default_state(const tstr global_folder) {
	FTPState* state = (FTPState*)malloc(sizeof(FTPState));

	if(!state) {
		return NULL;
	}

	size_t global_folder_length_1 = tstr_len(&global_folder);

	// invariant check
	if(tstr_cstr(&global_folder)[global_folder_length_1 - 1] == '/') {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelCritical, LogPrintLocation),
		                   "folder invariant 1 violated\n");

		free(state);
		return NULL;
	}

	char* const current_working_directory_impl = (char*)malloc(global_folder_length_1 + 1);

	if(!current_working_directory_impl) {
		free(state);
		return NULL;
	}

	memcpy(current_working_directory_impl, tstr_cstr(&global_folder), global_folder_length_1);
	current_working_directory_impl[global_folder_length_1] = '\0';

	state->current_working_directory =
	    tstr_own(current_working_directory_impl, global_folder_length_1, global_folder_length_1);

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

	SerializeResult16 port = serialize_u16_no_to_host(addr.port);

	IPV4RawBytes raw_ipv4_bytes = get_raw_bytes_as_host_bytes_from_ipv4_address(addr.addr);

	char* result = NULL;
	FORMAT_STRING(&result, return NULL;, "(%d,%d,%d,%d,%d,%d)", raw_ipv4_bytes.bytes[0],
	                                   raw_ipv4_bytes.bytes[1], raw_ipv4_bytes.bytes[2],
	                                   raw_ipv4_bytes.bytes[3], port.bytes[0], port.bytes[1]);

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
