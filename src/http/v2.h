

#pragma once

#include "./protocol.h"
#include "utils/buffered_reader.h"
#include "utils/utils.h"

#include <stdint.h>
#include <tvec.h>
#include <utils/sized_buffer.h>

// spec link:
// https://datatracker.ietf.org/doc/html/rfc7540

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

// see: https://datatracker.ietf.org/doc/html/rfc7540#section-7
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint32_t) {
	Http2ErrorCodeNoError = 0x0,
	Http2ErrorCodeProtocolError = 0x1,
	Http2ErrorCodeInternalError = 0x2,
	Http2ErrorCodeFlowControlError = 0x3,
	Http2ErrorCodeSettingsRimeout = 0x4,
	Http2ErrorCodeStreamClosed = 0x5,
	Http2ErrorCodeFrameSizeError = 0x6,
	Http2ErrorCodeRefusedStream = 0x7,
	Http2ErrorCodeCancel = 0x8,
	Http2ErrorCodeCompressionError = 0x9,
	Http2ErrorCodeConnectError = 0xa,
	Http2ErrorCodeEnhanceYourCalm = 0xb,
	Http2ErrorCodeInadequateSecurity = 0xc,
	Http2ErrorCodeHttp1Dot1Required = 0xd,
} Http2ErrorCode;

STATIC_ASSERT(sizeof(Http2ErrorCode) == sizeof(uint32_t), "Http2ErrorCode has to be 32 bits long!");

typedef struct {
	bool _reserved : 1; // for padding
	uint32_t last_stream_id : 31;
	Http2ErrorCode error_code;
	SizedBuffer additional_debug_data;
} Http2GoawayFrame;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	Http2SettingsFrameIdentifierHeaderTableSize = 0x1,
	Http2SettingsFrameIdentifierEnablePush = 0x2,
	Http2SettingsFrameIdentifierMaxConcurrentStreams = 0x3,
	Http2SettingsFrameIdentifierInitialWindowSize = 0x4,
	Http2SettingsFrameIdentifierMaxFrameSize = 0x5,
	Http2SettingsFrameIdentifierMaxHeaderListSize = 0x6,
} Http2SettingsFrameIdentifier;

STATIC_ASSERT(sizeof(Http2SettingsFrameIdentifier) == sizeof(uint16_t),
              "Http2SettingsFrameIdentifier has to be 16 bits long!");

typedef struct {
	Http2SettingsFrameIdentifier identifier;
	uint32_t value;
} Http2SettingSingleValue;

TVEC_DEFINE_VEC_TYPE(Http2SettingSingleValue)

typedef TVEC_TYPENAME(Http2SettingSingleValue) Http2SettingValues;

typedef struct {
	bool ack;
	Http2SettingValues entries;
} Http2SettingsFrame;

typedef struct {
	Http2FrameType type;
	union {
		Http2DataFrame data;
		int headers;
		int priority;
		int rst_stream;
		Http2SettingsFrame settings;
		int push_promise;
		int ping;
		Http2GoawayFrame goaway;
		int window_update;
		int continuation;
	} value;
} Http2Frame;

TVEC_DEFINE_VEC_TYPE(Http2Frame)

typedef TVEC_TYPENAME(Http2Frame) Http2Frames;

typedef struct {
	uint32_t header_table_size;
	bool enable_push;
	uint32_t max_concurrent_streams;
	uint32_t initial_window_size;
	uint32_t max_frame_size;
	uint32_t max_header_list_size;
} Http2Settings;

typedef struct {
	int todo;
} Http2PartialRequest;

TMAP_DEFINE_MAP_TYPE(size_t, Integer, Http2PartialRequest, Http2PartialRequestMap)

typedef TMAP_TYPENAME_MAP(Http2PartialRequestMap) Http2PartialRequestMap;

typedef struct {
	Http2Settings settings;
	Http2PartialRequestMap requests;
} HTTP2State;

NODISCARD HTTP2State http2_default_state(void);

NODISCARD HttpRequestResult parse_http2_request(HTTP2State* state, BufferedReader* reader);

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

NODISCARD int http2_send_stream_error(const ConnectionDescriptor* descriptor,
                                      Http2ErrorCode error_code, const char* error);

NODISCARD int http2_send_stream_error_with_data(const ConnectionDescriptor* descriptor,
                                                Http2ErrorCode error_code, SizedBuffer debug_data);
