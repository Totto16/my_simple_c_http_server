
#pragma once

#include "generic/secure.h"
#include "http/http_protocol.h"

typedef struct WSHandshakeResultImpl WSHandshakeResult;

WSHandshakeResult* handleWSHandshake(const HttpRequest* const httpRequest,
                                     const ConnectionDescriptor* const descriptor);

bool successfulWSHandshake(const WSHandshakeResult* const result);

int getWSHandshakeResult(WSHandshakeResult* result);

void freeWSHandshake(const WSHandshakeResult* const result);
