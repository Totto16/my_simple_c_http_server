

#pragma once

#include "generic/ip.h"
#include "http/send.h"
#include "utils/clock.h"

typedef struct {
	IPAddress host;
	char* ident;
	char* user;
	Time time;
	HttpRequestLine request;
	HttpStatusCode result;
	size_t body_size;
} LogEntry;

TVEC_DEFINE_VEC_TYPE(LogEntry)

typedef TVEC_TYPENAME(LogEntry) LogEntries;

typedef struct {
	LogEntries entries;
} LogCollector;

LogCollector* initialize_log_collector(void);

void free_log_collector(LogCollector* collector);

void log_collector_collect(LogCollector* collector, IPAddress address, HttpRequest http_request,
                           HTTPResponseToSend response);

NODISCARD StringBuilder* log_collector_to_string_builder(const LogCollector* collector);
