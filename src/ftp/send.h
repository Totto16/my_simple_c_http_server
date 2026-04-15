

#pragma once

#include "./protocol.h"
#include "generic/secure.h"
#include "utils/utils.h"

#include <tstr_builder.h>

NODISCARD GenericResult send_ftp_message_to_connection_tstr(const ConnectionDescriptor* descriptor,
                                                            FtpReturnCode status, tstr body);

NODISCARD GenericResult send_ftp_message_to_connection_buffer(
    const ConnectionDescriptor* descriptor, FtpReturnCode status, SizedBuffer buffer);

NODISCARD GenericResult send_ftp_message_to_connection_sb(const ConnectionDescriptor* descriptor,
                                                          FtpReturnCode status,
                                                          StringBuilder* body);
