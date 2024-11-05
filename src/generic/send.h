

#pragma once

#include "http/http_protocol.h"
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

typedef enum {
	CONNECTION_SEND_FLAGS_MALLOCED = 0b01,
	CONNECTION_SEND_FLAGS_UN_MALLOCED = 0b10

} CONNECTION_SEND_FLAGS;

NODISCARD int sendMessageToConnection(const ConnectionDescriptor* descriptor, int status,
                                      char* body, const char* MIMEType,
                                      HttpHeaderField* headerFields, int headerFieldsAmount,
                                      CONNECTION_SEND_FLAGS FLAGS);
