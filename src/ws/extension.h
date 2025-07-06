
#pragma once

#include "./types.h"
#include "utils/utils.h"

#include <stb/ds.h>

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WSExtensionTypePerMessageDeflate,
} WSExtensionType;

typedef struct {
	bool no_context_takeover;
	uint8_t max_window_bits;
} WsDeflateSingleOption;

typedef struct {
	WsDeflateSingleOption client;
	WsDeflateSingleOption server;
} WsDeflateOptions;

typedef struct {
	WSExtensionType type;
	union {
		WsDeflateOptions deflate;
	} data;
} WSExtension;

typedef STBDS_ARRAY(WSExtension) WSExtensions;

NODISCARD char* get_accepted_ws_extensions_as_string(WSExtensions extensions);

void parse_ws_extensions(WSExtensions* extensions, const char* const value_const);

typedef struct ExtensionReceivePipelineImpl ExtensionReceivePipeline;

typedef struct {
	uint8_t allowed_rsv_bytes;
} ExtensionReceivePipelineSettings;

typedef struct ExtensionMessageReceiveStateImpl ExtensionMessageReceiveState;

NODISCARD ExtensionReceivePipeline* get_extension_receive_pipeline(WSExtensions extensions);

NODISCARD ExtensionReceivePipelineSettings
get_extension_receive_pipeline_settings(const ExtensionReceivePipeline* extension_receive_pipeline);

NODISCARD ExtensionMessageReceiveState*
init_extension_receive_message_state(const ExtensionReceivePipeline* extension_receive_pipeline);

void free_extension_message_state(ExtensionMessageReceiveState* message_state);

NODISCARD char* extension_receive_pipeline_is_valid_cont_frame(
    const ExtensionReceivePipeline* extension_receive_pipeline,
    const ExtensionMessageReceiveState* message_state, WebSocketRawMessage message);

void extension_receive_pipeline_process_start_message(
    const ExtensionReceivePipeline* extension_receive_pipeline,
    ExtensionMessageReceiveState* message_state, WebSocketRawMessage message);

NODISCARD char* extension_receive_pipeline_process_finished_message(
    const ExtensionReceivePipeline* extension_receive_pipeline,
    const ExtensionMessageReceiveState* message_state, WebSocketMessage* message);
