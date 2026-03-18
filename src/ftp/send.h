

#pragma once

#include "./protocol.h"
#include "generic/secure.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

NODISCARD int send_ftp_message_to_connection_tstr(const ConnectionDescriptor* descriptor,
                                                  FtpReturnCode status, tstr body);

NODISCARD int send_ftp_message_to_connection_buffer(const ConnectionDescriptor* descriptor,
                                                    FtpReturnCode status, SizedBuffer buffer);

NODISCARD int send_ftp_message_to_connection_sb(const ConnectionDescriptor* descriptor,
                                                FtpReturnCode status, StringBuilder* body);
