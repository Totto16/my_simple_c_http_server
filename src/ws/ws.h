
#pragma once

#include "./extension.h"
#include "generic/secure.h"
#include "http/parser.h"
#include "http/protocol.h"

GENERATE_VARIANT_ALL_WS_FRAGMENT_OPTION()

typedef struct {
	WsFragmentOption fragment_option;
	WSExtensions extensions;
	bool trace;
} WsConnectionArgs;

NODISCARD GenericResult handle_ws_handshake(HttpRequest http_request,
                                            const ConnectionDescriptor* descriptor,
                                            HTTPGeneralContext* general_context,
                                            SendSettings send_settings, WSExtensions* extensions);

NODISCARD WsConnectionArgs get_ws_args_from_http_request(ParsedURLPath path,
                                                         WSExtensions extensions);
