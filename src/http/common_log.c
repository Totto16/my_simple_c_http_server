
#include "./common_log.h"

TVEC_IMPLEMENT_VEC_TYPE(LogEntry)

LogCollector* initialize_log_collector(void) {

	LogCollector* collector = malloc(sizeof(LogCollector));

	if(collector == NULL) {
		return NULL;
	}

	*collector = (LogCollector){
		.entries = TVEC_EMPTY(LogEntry),
	};

	return collector;
}

static void free_log_entry(LogEntry entry) {
	free(entry.ident);
	free(entry.user);
	free_parsed_request_uri(entry.request.uri);
}

void free_log_collector(LogCollector* collector) {

	for(size_t i = 0; i < TVEC_LENGTH(LogEntry, collector->entries); ++i) {
		LogEntry entry = TVEC_AT(LogEntry, collector->entries, i);
		free_log_entry(entry);
	}

	TVEC_FREE(LogEntry, &collector->entries);

	free(collector);
}

void log_collector_collect(LogCollector* collector, IPAddress address, HttpRequest http_request,
                           HTTPResponseToSend response) {

	Time now;

	bool success = get_current_time(&now);

	if(!success) {
		now = empty_time();
	}

	LogEntry entry = {
		.host = address,
		.ident = NULL,
		.user = NULL,
		.time = now,
		.request =
		    (HttpRequestLine){
		        .method = http_request.head.request_line.method,
		        .uri = duplicate_request_uri(http_request.head.request_line.uri),
		        .protocol_data = http_request.head.request_line.protocol_data,
		    },

		.result = response.status,
		.body_size = response.body.content.size,
	};

	auto _ = TVEC_PUSH(LogEntry, &collector->entries, entry);
	UNUSED(_);
}

NODISCARD static bool log_entry_to_string(StringBuilder* const builder, const LogEntry entry) {

	char* ip_str = ipv_to_string(entry.host);

	if(ip_str == NULL) {
		return false;
	}

	STRING_BUILDER_APPENDF(builder, return false;, "%s ", ip_str);
	free(ip_str);

	if(entry.ident != NULL) {
		STRING_BUILDER_APPENDF(builder, return false;, "%s ", entry.ident);
	} else {
		string_builder_append_single(builder, "- ");
	}

	if(entry.user != NULL) {
		STRING_BUILDER_APPENDF(builder, return false;, "%s ", entry.user);
	} else {
		string_builder_append_single(builder, "- ");
	}

	if(get_time_in_nano_seconds(entry.time) != 0) {
		char* time_str = get_date_string(entry.time, TimeFormatCommonLog);

		if(!time_str) {
			return false;
		}

		STRING_BUILDER_APPENDF(builder, return false;, "[%s] ", time_str);
		free(time_str);

	} else {
		string_builder_append_single(builder, "- ");
	}

	{ // request line

		const char* method_str = get_http_method_string(entry.request.method);

		char* uri_str = get_request_uri_as_string(entry.request.uri);

		const char* protocol_str =
		    get_http_protocol_version_string(entry.request.protocol_data.version);

		if(method_str == NULL || uri_str == NULL || protocol_str == NULL) {
			free(uri_str);
			return false;
		}

		STRING_BUILDER_APPENDF(builder, return false;
		                       , "\"%s %s %s\" ", method_str, uri_str, protocol_str);
	}

	if(entry.result != 0) {
		STRING_BUILDER_APPENDF(builder, return false;, "%d ", entry.result);
	} else {
		string_builder_append_single(builder, "- ");
	}

	STRING_BUILDER_APPENDF(builder, return false;, "%zu", entry.body_size);

	return true;
}

NODISCARD StringBuilder* log_collector_to_string_builder(const LogCollector* const collector) {

	StringBuilder* string_builder = string_builder_init();

	for(size_t i = 0; i < TVEC_LENGTH(LogEntry, collector->entries); ++i) {
		LogEntry entry = TVEC_AT(LogEntry, collector->entries, i);

		if(!log_entry_to_string(string_builder, entry)) {
			free_string_builder(string_builder);
			return NULL;
		}

		string_builder_append_single(string_builder, "\n");
	}

	return string_builder;
}
