
#include "./extension.h"

#include "utils/string_builder.h"

#include <ctype.h>

#define DEFAULT_MAX_WINDOW_BITS 15

#define MIN_MAX_WINDOW_BITS 8
#define MAX_MAX_WINDOW_BITS 15

NODISCARD static bool parse_ws_extension_per_message_deflate_params(char* params,
                                                                    WsDeflateOptions* options) {

	char* current_params = params;

	while(true) {

		char* next_params = index(current_params, ';');

		if(next_params != NULL) {
			*next_params = '\0';
		}

		// strip whitespace
		while(isspace(*current_params)) {
			current_params++;
		}

		{

			char* current_param_name = current_params;

			char* current_param_value = NULL;

			char* current_param_value_start = index(current_param_name, '=');

			if(current_param_value_start != NULL) {
				*current_param_value_start = '\0';
				current_param_value = current_param_value_start + 1;
			}

			if(strcmp(current_param_name, "server_no_context_takeover") == 0) {
				if(current_param_value != NULL) {
					return false;
				}

				options->server.no_context_takeover = true;

			} else if(strcmp(current_param_name, "server_max_window_bits") == 0) {

				uint8_t max_window_bits = DEFAULT_MAX_WINDOW_BITS;

				if(current_param_value != NULL) {
					bool success = true;
					long parsed_number = parse_long(current_param_value, &success);

					if(!success) {
						return false;
					}

					if(parsed_number < MIN_MAX_WINDOW_BITS || parsed_number > MAX_MAX_WINDOW_BITS) {
						return false;
					}

					max_window_bits = (uint8_t)parsed_number;
				}

				options->server.max_window_bits = max_window_bits;

			} else if(strcmp(current_param_name, "client_no_context_takeover") == 0) {
				if(current_param_value != NULL) {
					return false;
				}

				options->client.no_context_takeover = true;

			} else if(strcmp(current_param_name, "client_max_window_bits") == 0) {

				uint8_t max_window_bits = DEFAULT_MAX_WINDOW_BITS;

				if(current_param_value != NULL) {
					bool success = true;
					long parsed_number = parse_long(current_param_value, &success);

					if(!success) {
						return false;
					}

					if(parsed_number < 0 || parsed_number > UINT8_MAX) {
						return false;
					}

					max_window_bits = (uint8_t)parsed_number;
				}

				options->client.max_window_bits = max_window_bits;

			} else {
				return false;
			}
		}

		if(next_params == NULL) {
			break;
		}

		current_params = next_params + 1;
	}

	return true;
}

#define DEFAULT_CONTEXT_TAKEOVER_CLIENT_VALUE false

#define DEFAULT_CONTEXT_TAKEOVER_SERVER_VALUE true

NODISCARD static WSExtension parse_ws_extension_value(char* value, bool* success) {

	char* name = value;

	char* params_start = index(value, ';');

	char* params = NULL;
	if(params_start != NULL) {
		*params_start = '\0';
		params = params_start + 1;

		// strip whitespace
		while(isspace(*params)) {
			params++;
		}
	}

	if(strcmp(name, "permessage-deflate") == 0) {

		WSExtension extension = {
			.type = WSExtensionTypePerMessageDeflate,
			.data = { .deflate = { .client = { .no_context_takeover =
			                                       DEFAULT_CONTEXT_TAKEOVER_CLIENT_VALUE,
			                                   .max_window_bits = DEFAULT_MAX_WINDOW_BITS },
			                       .server = { .no_context_takeover =
			                                       DEFAULT_CONTEXT_TAKEOVER_SERVER_VALUE,
			                                   .max_window_bits = DEFAULT_MAX_WINDOW_BITS } } }
		};

		if(params != NULL) {
			bool res =
			    parse_ws_extension_per_message_deflate_params(params, &extension.data.deflate);

			*success = res;
			return extension;
		}

		*success = true;
		return extension;
	}

	*success = false;
	return (WSExtension){};
}

// see https://datatracker.ietf.org/doc/html/rfc6455#section-9.1
void parse_ws_extensions(WSExtensions* extensions, const char* const value_const) {

	char* value = strdup(value_const);

	char* current_extension = value;

	while(true) {

		char* next_value = index(current_extension, ',');

		if(next_value != NULL) {
			*next_value = '\0';
		}

		bool success = true;

		WSExtension extension = parse_ws_extension_value(current_extension, &success);

		if(success) {
			stbds_arrput(*extensions, extension);
		}

		if(next_value == NULL) {
			break;
		}

		current_extension = next_value + 1;
	}

	free(value);
}

NODISCARD static bool append_ws_extension_as_string(StringBuilder* string_builder,
                                                    WSExtension extension) {

	switch(extension.type) {
		case WSExtensionTypePerMessageDeflate: {

			string_builder_append_single(string_builder, "permessage-deflate;");

			WsDeflateOptions options = extension.data.deflate;

			// always return our window sizes
			size_t additional_options_count = 0;

			if(options.client.no_context_takeover) {
				additional_options_count++;
			}

			if(options.server.no_context_takeover) {
				additional_options_count++;
			}

			{

				string_builder_append_single(string_builder, "server_max_window_bits=");

				STRING_BUILDER_APPENDF(string_builder, return false;
				                       , "%d", options.server.max_window_bits);

				string_builder_append_single(string_builder, ";");
			}

			size_t index = 0;
			{

				string_builder_append_single(string_builder, "client_max_window_bits=");

				STRING_BUILDER_APPENDF(string_builder, return false;
				                       , "%d", options.client.max_window_bits);

				if(additional_options_count != index) {
					string_builder_append_single(string_builder, ";");
				}

				++index;
			}

			if(options.server.no_context_takeover) {

				string_builder_append_single(string_builder, "server_no_context_takeover");

				if(additional_options_count != index) {
					string_builder_append_single(string_builder, ";");
				}

				++index;
			}

			if(options.client.no_context_takeover) {

				string_builder_append_single(string_builder, "client_no_context_takeover");
			}

			break;
		}
		default: {
			return false;
		}
	}

	return true;
}

NODISCARD char* get_accepted_ws_extensions_as_string(WSExtensions extensions) {

	StringBuilder* string_builder = string_builder_init();

	if(!string_builder) {
		return NULL;
	}

	size_t extensions_length = stbds_arrlenu(extensions);

	for(size_t i = 0; i < extensions_length; ++i) {
		WSExtension extension = extensions[i];

		bool success = append_ws_extension_as_string(string_builder, extension);

		if(!success) {
			free_string_builder(string_builder);
			return NULL;
		}

		if(i != extensions_length - 1) {
			string_builder_append_single(string_builder, ",");
		}
	}

	return string_builder_release_into_string(&string_builder);
}

// note: ws spec defines how to handle multiple extensions and how to chain them, we only support
// one atm, but the chaining pipeline was still implemented in a way, so that no extension is fast
// and everything is handled correctly

// TODO:
typedef struct {
	int todo;
} WebSocketRawMessage;

typedef void (*WsProcessMessageRawFn)(WebSocketRawMessage* message, ANY arg);

typedef struct {
	WsProcessMessageRawFn ptr;
	ANY arg;
} WsProcessFn;

struct ExtensionPipelineImpl {
	ExtensionPipelineSettings settings;
	WsProcessFn process_fn;
};

NODISCARD static ExtensionPipelineSettings get_pipeline_settings(WSExtensions extensions) {
	ExtensionPipelineSettings settings = { .allowed_rsv_bytes = 0 };

	for(size_t i = 0; i < stbds_arrlenu(extensions); ++i) {
		WSExtension extension = extensions[i];

		switch(extension.type) {
			case WSExtensionTypePerMessageDeflate: {
				settings.allowed_rsv_bytes = settings.allowed_rsv_bytes & 0b100;
				break;
			}
			default: {
				break;
			}
		}
	}

	return settings;
}

// does't modify the message at all
static void noop_process_fn(WebSocketRawMessage* message, ANY_TYPE(NULL) arg) {
	UNUSED(message);
	UNUSED(arg);
}

typedef STBDS_ARRAY(WsProcessFn) ArrayProcessArg;

static void array_process_fn(WebSocketRawMessage* message, ANY_TYPE(ArrayProcessArg) arg) {

	ArrayProcessArg process_arg = (ArrayProcessArg)arg;

	for(size_t i = 0; i < stbds_arrlenu(process_arg); ++i) {
		WsProcessFn fn = process_arg[i];

		fn.ptr(message, fn.arg);
	}
}

static void permessage_deflate_process_fn(WebSocketRawMessage* message,
                                          ANY_TYPE(WsDeflateOptions*) arg) {

	WsDeflateOptions* process_arg = (WsDeflateOptions*)arg;

	// TODO
	UNUSED(message);
	UNUSED(process_arg);
}

NODISCARD ExtensionPipeline* get_extension_pipeline(WSExtensions extensions) {

	ExtensionPipeline* pipline = malloc(sizeof(ExtensionPipeline));

	if(!pipline) {
		return NULL;
	}

	pipline->settings = get_pipeline_settings(extensions);

	size_t extension_length = stbds_arrlenu(extensions);

	if(extension_length == 0) {
		pipline->process_fn.arg = NULL;
		pipline->process_fn.ptr = noop_process_fn;
		return pipline;
	}

	STBDS_ARRAY(WsProcessFn) array_fns = STBDS_ARRAY_EMPTY;

	for(size_t i = 0; i < extension_length; ++i) {
		WSExtension extension = extensions[i];

		WsProcessFn fn = {};
		switch(extension.type) {
			case WSExtensionTypePerMessageDeflate: {
				fn = (WsProcessFn){ .ptr = permessage_deflate_process_fn,
					                .arg = &extension.data.deflate };
				break;
			}
			default: {
				free(pipline);
				stbds_arrfree(array_fns);
				return NULL;
				break;
			}
		}

		stbds_arrput(array_fns, fn);
	}

	pipline->process_fn.ptr = array_process_fn;
	pipline->process_fn.arg = array_fns;

	return pipline;
}

NODISCARD ExtensionPipelineSettings
get_extension_pipeline_settings(ExtensionPipeline* extension_pipeline) {

	return extension_pipeline->settings;
}
