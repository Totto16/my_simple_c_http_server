

#pragma once

#include "utils/sized_buffer.h"
#include "utils/utils.h"

#include <stdint.h>

typedef struct {
	bool is_text;
	SizedBuffer data;
} WebSocketMessage;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	WsOpcodeCont = 0x0,
	WsOpcodeText = 0x1,
	WsOpcodeBin = 0x2,
	// 0x3 - 0x7 are reserved for further non-control frames
	WsOpcodeClose = 0x8,
	WsOpcodePing = 0x9,
	WsOpcodePong = 0xA,
	// 0xB - 0xF are reserved for further control frames
} WsOpcode;

static_assert(sizeof(uint64_t) == sizeof(size_t), "The payload size has to be 64 nits long");

typedef struct {
	bool fin;
	WsOpcode op_code;
	SizedBuffer payload;
	uint8_t rsv_bytes;
} WebSocketRawMessage;
