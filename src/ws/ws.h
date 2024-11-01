
#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

bool handleWSHandshake(const HttpRequest* const httpRequest,
                       const ConnectionDescriptor* const descriptor);
