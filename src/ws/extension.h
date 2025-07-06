
#pragma once

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
} ExtensionPipelineSettings;

NODISCARD ExtensionPipeline* get_extension_pipeline(WSExtensions extensions);

NODISCARD ExtensionPipelineSettings
get_extension_pipeline_settings(ExtensionPipeline* extension_pipeline);
