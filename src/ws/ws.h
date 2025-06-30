
#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

NODISCARD int handleWSHandshake(const HttpRequest* httpRequest,
                                const ConnectionDescriptor* descriptor, SendSettings send_settings);
