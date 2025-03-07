

#pragma once

#include "secure.h"
#include "utils/string_builder.h"

NODISCARD int sendDataToConnection(const ConnectionDescriptor* descriptor, void* toSend,
                                   size_t length);

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
NODISCARD int sendStringToConnection(const ConnectionDescriptor* descriptor, char* toSend);

// just a wrapper to send a string buffer to a connection, it also frees the string buffer!
NODISCARD int sendStringBuilderToConnection(const ConnectionDescriptor* descriptor,
                                            StringBuilder* stringBuilder);
