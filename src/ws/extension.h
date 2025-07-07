
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

typedef struct ExtensionPipelineImpl ExtensionPipeline;

typedef struct {
	uint8_t allowed_rsv_bytes;
} ExtensionReceivePipelineSettings;

typedef struct ExtensionMessageReceiveStateImpl ExtensionMessageReceiveState;

NODISCARD ExtensionPipeline* get_extension_pipeline(WSExtensions extensions);

void free_extension_pipeline(ExtensionPipeline* extension_pipeline);

NODISCARD ExtensionReceivePipelineSettings
get_extension_receive_pipeline_settings(const ExtensionPipeline* extension_pipeline);

NODISCARD ExtensionMessageReceiveState*
init_extension_receive_message_state(const ExtensionPipeline* extension_pipeline);

void free_extension_receive_message_state(ExtensionMessageReceiveState* message_state);

NODISCARD char*
extension_receive_pipeline_is_valid_cont_frame(const ExtensionPipeline* extension_pipeline,
                                               const ExtensionMessageReceiveState* message_state,
                                               WebSocketRawMessage message);

void extension_receive_pipeline_process_start_message(const ExtensionPipeline* extension_pipeline,
                                                      ExtensionMessageReceiveState* message_state,
                                                      WebSocketRawMessage message);

NODISCARD char* extension_receive_pipeline_process_finished_message(
    const ExtensionPipeline* extension_pipeline, const ExtensionMessageReceiveState* message_state,
    WebSocketMessage* message);

typedef struct ExtensionSendStateImpl ExtensionSendState;

NODISCARD ExtensionSendState*
pipline_get_extension_send_state(const ExtensionPipeline* extension_pipeline,
                                 const ExtensionMessageReceiveState* message_state);

void free_extension_send_state(ExtensionSendState* extension_send_state);

NODISCARD char*
extension_send_pipeline_process_initial_message(ExtensionSendState* extension_send_state,
                                                WebSocketMessage* message);

void extension_send_pipeline_process_start_message(ExtensionSendState* extension_send_state,
                                                   WebSocketRawMessage* raw_message);

void extension_send_pipeline_process_cont_message(ExtensionSendState* extension_send_state,
                                                  WebSocketRawMessage* raw_message);
