

#pragma once

#include "generic/secure.h"
#include "utils/utils.h"

typedef enum {
	CONNECTION_SEND_FLAGS_MALLOCED = 0b01,
	CONNECTION_SEND_FLAGS_UN_MALLOCED = 0b10

} CONNECTION_SEND_FLAGS;

NODISCARD int sendFTPMessageToConnection(const ConnectionDescriptor* descriptor, int status,
                                          char* body, CONNECTION_SEND_FLAGS FLAGS);
