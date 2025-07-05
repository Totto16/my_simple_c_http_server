
#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

NODISCARD int handle_ws_handshake(const HttpRequest* http_request,
                                  const ConnectionDescriptor* descriptor,
                                  SendSettings send_settings);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WsFragmentOptionTypeOff,
	WsFragmentOptionTypeAuto,
	WsFragmentOptionTypeSet
} WsFragmentOptionType;

typedef struct {
	WsFragmentOptionType type;
	union {
		struct {
			size_t fragment_size;
		} set;
	} data;
} WsFragmentOption;

typedef struct {
	WsFragmentOption fragment_option;
} WsConnectionArgs;

NODISCARD WsConnectionArgs get_ws_args_from_http_request(bool fragmented, ParsedURLPath path);
