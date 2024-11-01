

#pragma once

#include "http/http_protocol.h"
#include "secure.h"
#include "utils/string_builder.h"

void sendDataToConnection(const ConnectionDescriptor* const descriptor, void* toSend,
                          size_t length);

// sends a string to the connection, makes all write calls under the hood, deals with arbitrary
// large null terminated strings!
void sendStringToConnection(const ConnectionDescriptor* const descriptor, char* toSend);

// just a warpper to send a string buffer to a connection, it also frees the string buffer!
void sendStringBuilderToConnection(const ConnectionDescriptor* const descriptor,
                                   StringBuilder* stringBuilder);

typedef enum {
	CONNECTION_SEND_FLAGS_MALLOCED = 0b01,
	CONNECTION_SEND_FLAGS_UN_MALLOCED = 0b10

} CONNECTION_SEND_FLAGS;

void sendMessageToConnection(const ConnectionDescriptor* const descriptor, int status, char* body,
                             const char* MIMEType, HttpHeaderField* headerFields,
                             const int headerFieldsAmount, CONNECTION_SEND_FLAGS FLAGS);
