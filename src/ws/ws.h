
#pragma once

#include "./extension.h"
#include "generic/secure.h"
#include "http/protocol.h"
#include "http/parser.h"

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
	WSExtensions extensions;
	bool trace;
} WsConnectionArgs;

NODISCARD int handle_ws_handshake(HttpRequest http_request, const ConnectionDescriptor* descriptor,
                                  HTTPGeneralContext* general_context, SendSettings send_settings,
                                  WSExtensions* extension);

NODISCARD WsConnectionArgs get_ws_args_from_http_request(ParsedURLPath path,
                                                         WSExtensions extensions);
