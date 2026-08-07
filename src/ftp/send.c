

#include "./send.h"
#include "generic/send.h"

#define FTP_COMMAND_SEPERATOR "\r\n"

GenericResult send_ftp_message_to_connection_tstr( // NOLINT(misc-no-recursion)
    const ConnectionDescriptor* descriptor, FtpReturnCode status, tstr body) {

	if(status > InternalFtpReturnCodeMaximum || status < InternalFtpReturnCodeMinimum) {
		return send_ftp_message_to_connection_tstr(
		    descriptor, FtpReturnCodeSyntaxError,
		    TSTR_LIT("Internal Error while processing command: sending hardcoded invalid status"));
	}

	StringBuilder* string_builder = string_builder_init();

	if(tstr_len(&body) == 0) {
		STRING_BUILDER_APPENDF(string_builder, tstr_free(&body); return GENERIC_RES_ERR_UNIQUE();
		                       , "%03d%s", status, FTP_COMMAND_SEPERATOR);
	} else {
		STRING_BUILDER_APPENDF(string_builder, tstr_free(&body); return GENERIC_RES_ERR_UNIQUE();
		                       , "%03d " TSTR_FMT "%s", status, TSTR_FMT_ARGS(body),
		                       FTP_COMMAND_SEPERATOR);
	}

	const GenericResult result = send_string_builder_to_connection(descriptor, &string_builder);

	tstr_free(&body);
	return result;
}

NODISCARD GenericResult send_ftp_message_to_connection_buffer(
    const ConnectionDescriptor* descriptor, FtpReturnCode status, SizedBuffer buffer) {

	return send_ftp_message_to_connection_tstr(descriptor, status,
	                                           tstr_own(buffer.data, buffer.size, buffer.size));
}

// TODO(Totto): refactor ftp messages too, so that the return things are packed into a structure
GenericResult send_ftp_message_to_connection_sb(const ConnectionDescriptor* const descriptor,
                                                FtpReturnCode status, StringBuilder* body) {

	tstr val = string_builder_release_into_tstr(&body);

	const SizedBuffer buf = sized_buffer_from_tstr(&val);

	return send_ftp_message_to_connection_buffer(descriptor, status, buf);
}
