

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
	Http2FrameTypeData = 0x00,
	Http2FrameTypeHeaders = 0x01,
	Http2FrameTypePriority = 0x02,
	Http2FrameTypeRstStream = 0x03,
	Http2FrameTypeSettings = 0x04,
	Http2FrameTypePushPromise = 0x05,
	Http2FrameTypePing = 0x06,
	Http2FrameTypeGoaway = 0x07,
	Http2FrameTypeWindowUpdate = 0x08,
	Http2FrameTypeContinuation = 0x09,
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
