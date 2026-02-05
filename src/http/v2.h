

#pragma once

#include "./protocol.h"
#include "utils/buffered_reader.h"
#include "utils/utils.h"

#include <stdint.h>
#include <tvec.h>
#include <utils/sized_buffer.h>

// spec link:
// https://datatracker.ietf.org/doc/html/rfc7540

typedef struct {
	int todo;
} Http2Request;

// see: https://datatracker.ietf.org/doc/html/rfc7540#section-6
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2FrameType_DATA = 0x00,
	Http2FrameType_HEADERS = 0x01,
	Http2FrameType_PRIORITY = 0x02,
	Http2FrameType_RST_STREAM = 0x03,
	Http2FrameType_SETTINGS = 0x04,
	Http2FrameType_PUSH_PROMISE = 0x05,
	Http2FrameType_PING = 0x06,
	Http2FrameType_GOAWAY = 0x07,
	Http2FrameType_WINDOW_UPDATE = 0x08,
	Http2FrameType_CONTINUATION = 0x09,
} Http2FrameType;

typedef struct {
	SizedBuffer content;
} Http2DataFrame;

typedef struct {
	Http2FrameType type;
	union {
		Http2DataFrame data;
	} value;
} Http2Frame;

TVEC_DEFINE_VEC_TYPE(Http2Frame)

NODISCARD Http2Request* parse_http2_request_TODO(SizedBuffer raw_http_request);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2PrefaceStatusOk = 0,
	Http2PrefaceStatusErr,
	Http2PrefaceStatusNotEnoughData
} Http2PrefaceStatus;

NODISCARD Http2PrefaceStatus analyze_http2_preface(HttpRequestLine request_line,
                                                   BufferedReader* reader);
