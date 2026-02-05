

#pragma once

#include "./protocol.h"
#include "generic/secure.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

NODISCARD int send_ftp_message_to_connection_string(const ConnectionDescriptor* descriptor,
                                                    FtpReturnCode status, char* body);

NODISCARD int send_ftp_message_to_connection_single(const ConnectionDescriptor* descriptor,
                                                    FtpReturnCode status, const char* body);

NODISCARD int send_ftp_message_to_connection_sb(const ConnectionDescriptor* descriptor,
                                                FtpReturnCode status, StringBuilder* body);
