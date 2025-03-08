

#include "./send.h"
#include "generic/send.h"

static NODISCARD int sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
                                                     FTP_RETURN_CODE status, char* body) {

	if(status > INTERNAL_FTP_RETURN_CODE_MAXIMUM || status < INTERNAL_FTP_RETURN_CODE_MINIMUM) {
		return sendFTPMessageToConnection(
		    descriptor, FTP_RETURN_CODE_SYNTAX_ERROR,
		    "Internal Error while processing command: sending hardcoded invalid status",
		    CONNECTION_SEND_FLAGS_UN_MALLOCED);
	}

	StringBuilder* sb = string_builder_init();
	const char* const separators = "\r\n";

	string_builder_append(sb, return -3;, "%03d %s%s", status, body, separators);

	int result = sendStringBuilderToConnection(descriptor, sb);

	free(body);
	return result;
}

int sendFTPMessageToConnection(const ConnectionDescriptor* descriptor, FTP_RETURN_CODE status, char* body,
                               CONNECTION_SEND_FLAGS FLAGS) {
	char* final_body = body;

	if((FLAGS & CONNECTION_SEND_FLAGS_UN_MALLOCED) != 0) {
		if(body) {
			final_body = normalStringToMalloced(body);
		}
	}

	return sendMessageToConnectionMalloced(descriptor, status, final_body);
}

int sendFTPMessageToConnectionSb(const ConnectionDescriptor* const descriptor,
                                 FTP_RETURN_CODE status, StringBuilder* body) {
	return sendFTPMessageToConnection(descriptor, status, string_builder_to_string(body),
	                                  CONNECTION_SEND_FLAGS_MALLOCED);
}
