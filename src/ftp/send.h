

#pragma once

#include "./protocol.h"
#include "generic/secure.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

/**
 * @enum MASK / FLAGS
 */
typedef enum {
	CONNECTION_SEND_FLAGS_MALLOCED = 0b01,
	CONNECTION_SEND_FLAGS_UN_MALLOCED = 0b10

} CONNECTION_SEND_FLAGS;

NODISCARD int sendFTPMessageToConnection(const ConnectionDescriptor* descriptor,
                                         FTP_RETURN_CODE status, char* body,
                                         CONNECTION_SEND_FLAGS FLAGS);

NODISCARD int sendFTPMessageToConnectionSb(const ConnectionDescriptor* descriptor,
                                           FTP_RETURN_CODE status, StringBuilder* body);
