
#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

NODISCARD int handleWSHandshake(const HttpRequest* const httpRequest,
                                const ConnectionDescriptor* const descriptor);
