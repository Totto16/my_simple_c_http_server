

#pragma once

#include "./hpack.h"
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
	bool _reserved : 1;
	uint32_t identifier : 31; // only 31 bits!
} Http2Identifier;

typedef struct {
	SizedBuffer content;
	bool is_end;
	Http2Identifier identifier;
} Http2DataFrame;

typedef struct {
	bool exclusive : 1;
	uint32_t dependency_identifier : 31;
	uint8_t weight;
} Http2FramePriority;

typedef struct {
	bool has_priority;
	Http2FramePriority priority;
} Http2FramePriorityOptional;

typedef struct {
	Http2FramePriorityOptional priority_opt;
	SizedBuffer block_fragment;
	Http2Identifier identifier;
	bool end_stream;
	bool end_headers;
} Http2HeadersFrame;

typedef struct {
	Http2FramePriority priority;
	Http2Identifier identifier;
} Http2PriorityFrame;

// see: https://datatracker.ietf.org/doc/html/rfc7540#section-7
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint32_t) {
	Http2ErrorCodeNoError = 0x0,
	Http2ErrorCodeProtocolError = 0x1,
	Http2ErrorCodeInternalError = 0x2,
	Http2ErrorCodeFlowControlError = 0x3,
	Http2ErrorCodeSettingsTimeout = 0x4,
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
	Http2ErrorCode error_code;
	Http2Identifier identifier;
} Http2RstStreamFrame;

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
	Http2Identifier promised_stream_identifier;
	SizedBuffer block_fragment;
	Http2Identifier identifier;
	bool end_headers;
} Http2PushPromiseFrame;

typedef struct {
	bool ack;
	SizedBuffer opaque_data;
} Http2PingFrame;

typedef struct {
	Http2Identifier last_stream_id;
	Http2ErrorCode error_code;
	SizedBuffer additional_debug_data;
} Http2GoawayFrame;

typedef struct {
	bool _reserved : 1; // for padding
	uint32_t window_size_increment : 31;
	Http2Identifier identifier;
} Http2WindowUpdateFrame;

typedef struct {
	SizedBuffer block_fragment;
	Http2Identifier identifier;
	bool end_headers;
} Http2ContinuationFrame;

typedef struct {
	Http2FrameType type;
	union {
		Http2DataFrame data;
		Http2HeadersFrame headers;
		Http2PriorityFrame priority;
		Http2RstStreamFrame rst_stream;
		Http2SettingsFrame settings;
		Http2PushPromiseFrame push_promise;
		Http2PingFrame ping;
		Http2GoawayFrame goaway;
		Http2WindowUpdateFrame window_update;
		Http2ContinuationFrame continuation;
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

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	Http2StreamStateIdle = 0,
	Http2StreamStateReserved,
	Http2StreamStateOpen,
	Http2StreamStateHalfClosed,
	Http2StreamStateClosed
} Http2StreamState;

TVEC_DEFINE_VEC_TYPE(SizedBuffer)

typedef TVEC_TYPENAME(SizedBuffer) DataBlocks;

typedef struct {
	bool finished;
	DataBlocks header_blocks;
} Http2StreamHeaders;

typedef struct {
	bool finished;
	DataBlocks data_blocks;
} Http2StreamContent;

typedef struct {
	Http2StreamState state;
	// data, depending on state,is always there, but may be empty
	Http2StreamHeaders headers;
	Http2StreamContent content;
	bool end_stream;
	Http2FramePriority priority;
} Http2Stream;

// TODO: => v
//  Http2Stream is either a rec or send request, depending if th identifier is odd or even,
//  also, keep track of the last stream id in the state, so that creating and sending a new one is
//  easy
// TODO: make sure that the new send identifier are even and the received are odd!

TMAP_DEFINE_MAP_TYPE(Http2Identifier, StreamIdentifier, Http2Stream, Http2StreamMap)

typedef TMAP_TYPENAME_MAP(Http2StreamMap) Http2StreamMap;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	Http2StateTypeOpen = 0,
	Http2StateTypeClosed,
} Http2StateType;

typedef struct {
	Http2StateType type;
	union {
		Http2ErrorCode closed_reason;
	} value;
} Http2State;

typedef struct {
	Http2Identifier last_stream_id;
	Http2State state;
	HpackState* hpack_state;
} Http2ContextState;

typedef struct {
	Http2Settings settings;
	Http2StreamMap streams;
	Http2Frames frames;
	Http2ContextState state;
} HTTP2Context;

NODISCARD HTTP2Context http2_default_context(void);

NODISCARD HttpRequestResult parse_http2_request(HTTP2Context* context, BufferedReader* reader);

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

typedef struct {
	bool is_error;
	union {
		const char* error;
	} value;
} Http2StartResult;

NODISCARD Http2StartResult http2_send_and_receive_preface(HTTP2Context* context,
                                                          BufferedReader* reader);

NODISCARD int http2_send_connection_error(const ConnectionDescriptor* descriptor,
                                          Http2ErrorCode error_code, const char* error);

NODISCARD int http2_send_connection_error_with_data(const ConnectionDescriptor* descriptor,
                                                    Http2ErrorCode error_code,
                                                    SizedBuffer debug_data);

NODISCARD int http2_send_stream_error(const ConnectionDescriptor* descriptor,
                                      Http2ErrorCode error_code, Http2Identifier stream_identifier);

void free_http2_context(HTTP2Context context);

NODISCARD int http2_send_headers(const ConnectionDescriptor* descriptor, Http2Identifier identifier,
                                 Http2Settings settings, SizedBuffer buffer,
                                 bool headers_are_end_stream);

NODISCARD int http2_send_data(const ConnectionDescriptor* descriptor, Http2Identifier identifier,
                              Http2Settings settings, SizedBuffer buffer);

NODISCARD Http2Identifier get_new_http2_identifier(HTTP2Context* context);
