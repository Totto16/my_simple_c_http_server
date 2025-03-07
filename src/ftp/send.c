

#include "./send.h"
#include "generic/send.h"

static NODISCARD int sendMessageToConnectionMalloced(const ConnectionDescriptor* const descriptor,
                                                     int status, char* body) {

	if(status >= 1000) {
		return sendFTPMessageToConnection(descriptor, 550, "TOD",
		                                  CONNECTION_SEND_FLAGS_UN_MALLOCED);
	}

	StringBuilder* sb = string_builder_init();
	const char* const separators = "\r\n";

	string_builder_append(sb, return -3;, "%d %s%s", status, body, separators);

	int result = sendStringBuilderToConnection(descriptor, sb);

	free(body);
	return result;
}

int sendFTPMessageToConnection(const ConnectionDescriptor* descriptor, int status, char* body,
                               CONNECTION_SEND_FLAGS FLAGS) {
	char* final_body = body;

	if((FLAGS & CONNECTION_SEND_FLAGS_UN_MALLOCED) != 0) {
		if(body) {
			final_body = normalStringToMalloced(body);
		}
	}

	return sendMessageToConnectionMalloced(descriptor, status, final_body);
}
