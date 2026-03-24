

#pragma once

#include "secure.h"
#include "utils/string_builder.h"

NODISCARD GenericResult send_data_to_connection(const ConnectionDescriptor* descriptor,
                                                void* to_send, size_t length);

NODISCARD GenericResult send_buffer_to_connection(const ConnectionDescriptor* descriptor,
                                                  SizedBuffer buffer);

// just a wrapper to send a string buffer to a connection, it also frees the string buffer!
NODISCARD GenericResult send_string_builder_to_connection(const ConnectionDescriptor* descriptor,
                                                          StringBuilder** string_builder);
