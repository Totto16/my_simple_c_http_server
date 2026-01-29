

#pragma once

#include "utils/utils.h"

#include <stb/ds.h>
#include <stdint.h>
#include <utils/sized_buffer.h>

// spec link:
// https://datatracker.ietf.org/doc/html/rfc7540

// see: https://datatracker.ietf.org/doc/html/rfc7540#section-3.5

#define HTTP2_CLIENT_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"

static const char* const g_http2_client_preface = HTTP2_CLIENT_PREFACE;

static const uint8_t g_http2_client_preface_length = sizeof(HTTP2_CLIENT_PREFACE) - 1;

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

typedef STBDS_ARRAY(Http2Frame) Http2Frames;

NODISCARD Http2Request* parse_http_request(SizedBuffer raw_http_request);
