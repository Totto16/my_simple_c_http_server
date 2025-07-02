

#pragma once

#include "./protocol.h"
#include "generic/secure.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ConnectionSendFlagsMalloced = 0b01,
	ConnectionSendFlagsUnMalloced = 0b10
} ConnectionSendFlags;

NODISCARD int send_ftp_message_to_connection(const ConnectionDescriptor* descriptor,
                                             FtpReturnCode status, char* body,
                                             ConnectionSendFlags flags);

NODISCARD int send_ftp_message_to_connection_sb(const ConnectionDescriptor* descriptor,
                                                FtpReturnCode status, StringBuilder* body);
