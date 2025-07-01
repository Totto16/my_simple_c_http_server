
#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

NODISCARD int handle_ws_handshake(const HttpRequest* http_request,
                                  const ConnectionDescriptor* descriptor,
                                  SendSettings send_settings);
