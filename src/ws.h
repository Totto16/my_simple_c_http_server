
#pragma once

#include "http_protocol.h"
#include "secure.h"

typedef struct WSHandshakeResultImpl WSHandshakeResult;

WSHandshakeResult* handleWSHandshake(const HttpRequest* const httpRequest,
                                     const ConnectionDescriptor* const descriptor);

bool successfulWSHandshake(const WSHandshakeResult* const result);

int getWSHandshakeResult(WSHandshakeResult* result);

void freeWSHandshake(const WSHandshakeResult* const result);
