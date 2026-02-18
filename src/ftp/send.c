

#include "./send.h"
#include "generic/send.h"

#define FTP_COMMAND_SEPERATOR "\r\n"

NODISCARD static int send_ftp_message_to_connection_malloced( // NOLINT(misc-no-recursion)
    const ConnectionDescriptor* const descriptor, FtpReturnCode status, char* body) {

	if(status > InternalFtpReturnCodeMaximum || status < InternalFtpReturnCodeMinimum) {
		return send_ftp_message_to_connection_single(
		    descriptor, FtpReturnCodeSyntaxError,
		    "Internal Error while processing command: sending hardcoded invalid status");
	}

	StringBuilder* string_builder = string_builder_init();

	if(strlen(body) == 0) {
		STRING_BUILDER_APPENDF(string_builder, free(body); return -3;
		                       , "%03d%s", status, FTP_COMMAND_SEPERATOR);
	} else {
		STRING_BUILDER_APPENDF(string_builder, free(body); return -3;
		                       , "%03d %s%s", status, body, FTP_COMMAND_SEPERATOR);
	}

	int result = send_string_builder_to_connection(descriptor, &string_builder);

	free(body);
	return result;
}

int send_ftp_message_to_connection_string( // NOLINT(misc-no-recursion)
    const ConnectionDescriptor* descriptor, FtpReturnCode status, char* const body) {

	return send_ftp_message_to_connection_malloced(descriptor, status, body);
}

int send_ftp_message_to_connection_single( // NOLINT(misc-no-recursion)
    const ConnectionDescriptor* descriptor, FtpReturnCode status, const char* const body) {

	char* final_body = NULL;

	if(body) {
		final_body = strdup(body);
	}

	return send_ftp_message_to_connection_malloced(descriptor, status, final_body);
}

// TODO(Totto): refactor ftp messages too, so that the return things ar epavcked into a structure
int send_ftp_message_to_connection_sb(const ConnectionDescriptor* const descriptor,
                                      FtpReturnCode status, StringBuilder* body) {
	return send_ftp_message_to_connection_string(descriptor, status,
	                                             string_builder_release_into_string(&body));
}
