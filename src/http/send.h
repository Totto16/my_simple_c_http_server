

#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	CONNECTION_SEND_FLAGS_MALLOCED = 0b01,
	CONNECTION_SEND_FLAGS_UN_MALLOCED = 0b10

} CONNECTION_SEND_FLAGS;

NODISCARD int sendHTTPMessageToConnection(const ConnectionDescriptor* descriptor, int status,
                                          char* body, const char* MIMEType,
                                          HttpHeaderField* headerFields, int headerFieldsAmount,
                                          CONNECTION_SEND_FLAGS FLAGS, SendSettings send_settings);

// TODO: enforce styles e.g. camelcase or snake:_case
//  factor our send message into struct, so that there arent 7 paramnaters
//  use ds_arr everywhere (realloc)
