
#include "./extension.h"

#include "http/compression.h"
#include "utils/string_builder.h"

#include <ctype.h>

#define DEFAULT_MAX_WINDOW_BITS 15

#define MIN_MAX_WINDOW_BITS 9
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

typedef char* (*WsProcessReceiveMessageFn)(WebSocketMessage* message,
                                           const ExtensionMessageReceiveState* const message_state,
                                           ANY arg);

typedef char* (*WsProcessSendMessageRawFn)(WebSocketRawMessage* raw_message,
                                           const ExtensionMessageReceiveState* message_state,
                                           ANY arg);

typedef struct {
	WsProcessReceiveMessageFn receive_fn;
	WsProcessSendMessageRawFn send_fn;
	ANY arg;
} WsProcessFn;

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WsExtensionMaskNone = 0x00,
	WsExtensionMaskPerMessageDeflate = 0x01,
} WsExtensionMask;

struct ExtensionPipelineImpl {
	ExtensionReceivePipelineSettings settings;
	WsProcessFn process_fn;
	size_t active_extensions;
	WsExtensionMask extension_mask;
};

struct ExtensionMessageReceiveStateImpl {
	bool is_compressed_message;
};

NODISCARD static ExtensionReceivePipelineSettings get_pipeline_settings(WSExtensions extensions) {
	ExtensionReceivePipelineSettings settings = { .allowed_rsv_bytes = 0 };

	for(size_t i = 0; i < stbds_arrlenu(extensions); ++i) {
		WSExtension extension = extensions[i];

		switch(extension.type) {
			case WSExtensionTypePerMessageDeflate: {
				settings.allowed_rsv_bytes = settings.allowed_rsv_bytes | 0b100;
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
static char* noop_process_receive_fn(WebSocketMessage* message,
                                     const ExtensionMessageReceiveState* const message_state,
                                     ANY_TYPE(NULL) arg) {
	UNUSED(message);
	UNUSED(message_state);
	UNUSED(arg);
	return NULL;
}

static char* noop_process_send_fn(WebSocketRawMessage* raw_message,
                                  const ExtensionMessageReceiveState* message_state,
                                  ANY_TYPE(NULL) arg) {
	UNUSED(raw_message);
	UNUSED(arg);
	UNUSED(message_state);
	return NULL;
}

typedef STBDS_ARRAY(WsProcessFn) ArrayProcessArg;

static char* array_process_receive_fn(WebSocketMessage* message,
                                      const ExtensionMessageReceiveState* const message_state,
                                      ANY_TYPE(ArrayProcessReceiveArg) arg) {

	ArrayProcessArg process_arg = (ArrayProcessArg)arg;

	size_t process_arg_lenght = stbds_arrlenu(process_arg);

	if(process_arg_lenght == 0) {
		return NULL;
	}

	// NOTE: receive extensions are run in reverse

	for(size_t i = process_arg_lenght - 1; i >= 0; --i) {
		WsProcessFn process_fn = process_arg[i];

		char* error = process_fn.receive_fn(message, message_state, process_fn.arg);
		if(error != NULL) {
			return error;
		}
	}
	return NULL;
}

static char* array_process_send_fn(WebSocketRawMessage* raw_message,
                                   const ExtensionMessageReceiveState* message_state,
                                   ANY_TYPE(ArrayProcessReceiveArg) arg) {

	ArrayProcessArg process_arg = (ArrayProcessArg)arg;

	for(size_t i = 0; i < stbds_arrlenu(process_arg); ++i) {
		WsProcessFn process_fn = process_arg[i];

		char* error = process_fn.send_fn(raw_message, message_state, process_fn.arg);
		if(error != NULL) {
			return error;
		}
	}
	return NULL;
}

static char* decompress_ws_message(WebSocketMessage* message, WsDeflateOptions* options) {
	// see: https://datatracker.ietf.org/doc/html/rfc7692#section-6.2

	SizedBuffer input_buffer = { .data = message->data, .size = message->data_len };

	if(!options->client.no_context_takeover) {
		return strdup("client context takeover is not supported");
	}

	SizedBuffer result =
	    decompress_buffer_with_zlib(input_buffer, false, options->client.max_window_bits);

	if(result.data == NULL) {
		return strdup("decompress error");
	}

	free_sized_buffer(input_buffer);

	message->data = result.data;
	message->data_len = result.size;

	return NULL;
}

static char* compress_ws_message(WebSocketRawMessage* raw_message, WsDeflateOptions* options) {
	// see: https://datatracker.ietf.org/doc/html/rfc7692#section-6.1

	SizedBuffer input_buffer = { .data = raw_message->payload, .size = raw_message->payload_len };

	if(!options->server.no_context_takeover) {
		return strdup("server context takeover is not supported");
	}

	SizedBuffer result =
	    compress_buffer_with_zlib(input_buffer, false, options->server.max_window_bits);

	if(result.data == NULL) {
		return strdup("compress error");
	}

	free_sized_buffer(input_buffer);

	raw_message->payload = result.data;
	raw_message->payload_len = result.size;
	raw_message->rsv_bytes = raw_message->rsv_bytes | 0b100;

	return NULL;
}

static char*
permessage_deflate_process_receive_fn(WebSocketMessage* message,
                                      const ExtensionMessageReceiveState* const message_state,
                                      ANY_TYPE(WsDeflateOptions*) arg) {

	if(!message_state->is_compressed_message) {
		return NULL;
	}

	WsDeflateOptions* process_arg = (WsDeflateOptions*)arg;

	return decompress_ws_message(message, process_arg);
}

static char* permessage_deflate_process_send_fn(WebSocketRawMessage* raw_message,
                                                const ExtensionMessageReceiveState* message_state,
                                                ANY_TYPE(WsDeflateOptions*) arg) {

	if(!message_state->is_compressed_message) {
		return NULL;
	}

	WsDeflateOptions* process_arg = (WsDeflateOptions*)arg;

	return compress_ws_message(raw_message, process_arg);
}

NODISCARD ExtensionPipeline* get_extension_pipeline(WSExtensions extensions) {

	ExtensionPipeline* extension_pipeline = malloc(sizeof(ExtensionPipeline));

	if(!extension_pipeline) {
		return NULL;
	}

	extension_pipeline->settings = get_pipeline_settings(extensions);
	extension_pipeline->extension_mask = WsExtensionMaskNone;

	size_t extension_length = stbds_arrlenu(extensions);
	extension_pipeline->active_extensions = extension_length;

	if(extension_length == 0) {
		extension_pipeline->process_fn.arg = NULL;
		extension_pipeline->process_fn.receive_fn = noop_process_receive_fn;
		extension_pipeline->process_fn.send_fn = noop_process_send_fn;
		return extension_pipeline;
	}

	STBDS_ARRAY(WsProcessFn) array_fns = STBDS_ARRAY_EMPTY;

	for(size_t i = 0; i < extension_length; ++i) {
		WSExtension* extension = &(extensions[i]);

		WsProcessFn process_fn = {};
		switch(extension->type) {
			case WSExtensionTypePerMessageDeflate: {
				process_fn = (WsProcessFn){ .receive_fn = permessage_deflate_process_receive_fn,
					                        .send_fn = permessage_deflate_process_send_fn,
					                        .arg = &(extension->data.deflate) };
				extension_pipeline->extension_mask =
				    extension_pipeline->extension_mask | WsExtensionMaskPerMessageDeflate;
				break;
			}
			default: {
				free(extension_pipeline);
				stbds_arrfree(array_fns);
				return NULL;
				break;
			}
		}

		stbds_arrput(array_fns, process_fn);
	}

	extension_pipeline->process_fn.receive_fn = array_process_receive_fn;
	extension_pipeline->process_fn.send_fn = array_process_send_fn;
	extension_pipeline->process_fn.arg = array_fns;

	return extension_pipeline;
}

NODISCARD ExtensionReceivePipelineSettings
get_extension_receive_pipeline_settings(const ExtensionPipeline* const extension_pipeline) {

	return extension_pipeline->settings;
}

NODISCARD ExtensionMessageReceiveState*
init_extension_receive_message_state(const ExtensionPipeline* const extension_pipeline) {

	if(extension_pipeline->active_extensions == 0) {
		return NULL;
	}

	ExtensionMessageReceiveState* message_state = malloc(sizeof(ExtensionMessageReceiveState));

	if(!message_state) {
		return NULL;
	}

	message_state->is_compressed_message = false;

	return message_state;
}

void free_extension_message_state(ExtensionMessageReceiveState* message_state) {
	if(message_state != NULL) {
		free(message_state);
	}
}

NODISCARD char* extension_receive_pipeline_is_valid_cont_frame(
    const ExtensionPipeline* const extension_pipeline,
    const ExtensionMessageReceiveState* const message_state, const WebSocketRawMessage message) {

	if(extension_pipeline->active_extensions == 0) {
		return NULL;
	}

	// this may be used in other extensions
	UNUSED(message_state);

	if((extension_pipeline->extension_mask & WsExtensionMaskPerMessageDeflate) != 0) {

		if((message.rsv_bytes & 0b100) != 0) {
			return strdup(
			    "Received Opcode CONTINUATION with compressed bit set, this is an extension "
			    "error");
		}
	}

	return NULL;
}

void extension_receive_pipeline_process_start_message(
    const ExtensionPipeline* const extension_pipeline,
    ExtensionMessageReceiveState* const message_state, const WebSocketRawMessage message) {

	if(extension_pipeline->active_extensions == 0) {
		return;
	}

	if((extension_pipeline->extension_mask & WsExtensionMaskPerMessageDeflate) != 0) {
		message_state->is_compressed_message = (message.rsv_bytes & 0b100) != 0;
	}
}

NODISCARD char* extension_receive_pipeline_process_finished_message(
    const ExtensionPipeline* const extension_pipeline,
    const ExtensionMessageReceiveState* const message_state, WebSocketMessage* message) {

	if(extension_pipeline->active_extensions == 0) {
		return NULL;
	}

	return extension_pipeline->process_fn.receive_fn(message, message_state,
	                                                 extension_pipeline->process_fn.arg);
}

struct ExtensionSendStateImpl {
	const ExtensionPipeline* extension_pipeline;
	const ExtensionMessageReceiveState* message_state;
};

NODISCARD ExtensionSendState*
pipline_get_extension_send_state(const ExtensionPipeline* extension_pipeline,
                                 const ExtensionMessageReceiveState* message_state) {

	ExtensionSendState* extension_send_state = malloc(sizeof(ExtensionSendState));

	if(!extension_send_state) {
		return NULL;
	}

	extension_send_state->extension_pipeline = extension_pipeline;
	extension_send_state->message_state = message_state;

	return extension_send_state;
}

NODISCARD char*
extension_send_pipeline_process_finished_message(ExtensionSendState* extension_send_state,
                                                 WebSocketRawMessage* raw_message) {

	if(extension_send_state->extension_pipeline->active_extensions == 0) {
		return NULL;
	}

	return extension_send_state->extension_pipeline->process_fn.send_fn(
	    raw_message, extension_send_state->message_state,
	    extension_send_state->extension_pipeline->process_fn.arg);
}
