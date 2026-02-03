

#include "./v2.h"

TVEC_IMPLEMENT_VEC_TYPE(Http2Frame)

#define HTTP2_HEADER_SIZE 9

typedef struct {
	uint32_t length : 24; // site without this header
	uint8_t type;
	uint8_t flags;
	uint32_t stream_identifier; // only 31 bits!
} Http2RawHeader;

NODISCARD static Http2RawHeader parse_http2_raw_header(const uint8_t* const header_data) {

	uint8_t type = header_data[3];

	uint8_t flags = header_data[4];

	// this is big endian!
	uint32_t length = (header_data[0] << 16) | (header_data[1] << 8) | (header_data[2]);

	uint32_t stream_identifier_raw;

	memcpy(&stream_identifier_raw, header_data + 5, sizeof(stream_identifier_raw));

	// take the first 31 bit from the 32 bit value
	uint32_t stream_identifier = stream_identifier_raw & 0x7FFFFFFF;

	// MUST be ignored in receiving
	// bool reserved = ((stream_identifier_raw >> 31) & 1) == 1;

	Http2RawHeader result = (Http2RawHeader){
		.length = length, .type = type, .flags = flags, .stream_identifier = stream_identifier
	};

	return result;
}

typedef struct {
	bool has_padding;
	uint8_t padding_length;
	SizedBuffer content;
	SizedBuffer padding;
} Http2DataFrameRaw;

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2FrameFlag_END_STREAM = 0x01,
	Http2FrameFlag_END_HEADERS = 0x04,
	Http2FrameFlag_PADDED = 0x08,
	Http2FrameFlag_PRIORITY = 0x20,
} Http2FrameFlag;

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2DataFrameFlag_END_STREAM = Http2FrameFlag_END_STREAM,
	Http2DataFrameFlag_PADDED = Http2FrameFlag_PADDED,
} Http2DataFrameFlag;

// TODO
// psrse data fraem raw,
//  if the flags have the padding bit set, read padding length,
//  read content. read padding, verify, that the size is a expewcted,
//  optionally check the padding bytes to be 0!

//  DATA frames MUST be associated with a stream.  If a DATA frame is
//    received whose stream identifier field is 0x0, the recipient MUST
//    respond with a connection error (Section 5.4.1) of type
//    PROTOCOL_ERROR.

#define DEFAULT_SETTINGS_MAX_FRAME_SIZE (1 << 14) // 2^14

// TODO: accept parsstate and  http2 state, e.g. the SETTINgs is in there!
NODISCARD Http2Request* parse_http2_request_TODO(SizedBuffer raw_http_request) {

	TVEC_TYPENAME(Http2Frame) frames = TVEC_EMPTY(Http2Frame);

	// TODO: Support the SETTINGS_MAX_FRAME_SIZE setting, in some cases, the settings header needs
	// to be sent as first, or in h2c cases, it is required to supply it  via a http 1 header

	//

	size_t remaining_data = raw_http_request.size;

	uint8_t* start_data = (uint8_t*)raw_http_request.data;

	do {

		if(remaining_data < HTTP2_HEADER_SIZE) {
			// TODO: return proper error!
			return NULL;
		}

		const Http2RawHeader http2_raw_header = parse_http2_raw_header(start_data + remaining_data);

		if(http2_raw_header.length > DEFAULT_SETTINGS_MAX_FRAME_SIZE) {
			// TODO
			return NULL;
		}

		Http2Frame* frame = NULL;

		switch(http2_raw_header.type) {
			case Http2FrameType_DATA: {
				break;
			}
			case Http2FrameType_HEADERS: {
				break;
			}
			case Http2FrameType_PRIORITY: {
				break;
			}
			case Http2FrameType_RST_STREAM: {
				break;
			}
			case Http2FrameType_SETTINGS: {
				break;
			}
			case Http2FrameType_PUSH_PROMISE: {
				break;
			}
			case Http2FrameType_PING: {
				break;
			}
			case Http2FrameType_GOAWAY: {
				break;
			}
			case Http2FrameType_WINDOW_UPDATE: {
				break;
			}
			case Http2FrameType_CONTINUATION: {
				break;
			}
			default: {
				assert(false && "TODO");
				// TODO
				// return TODO;
			}
		}

		if(frame == NULL) {
			assert(false && "TODO");
			// TODO
			// return TODO;
		}

		// TODO
		auto _ = TVEC_PUSH(Http2Frame, &frames, *frame);
		// TODO
		UNUSED(_);

	} while(remaining_data != 0);

	assert(false && "TODO");
	// TODO
	// return frames;
}

// http2 preface: see: https://datatracker.ietf.org/doc/html/rfc7540#section-3.5

#define HTTP2_CLIENT_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"

#define SIZEOF_HTTP2_CLIENT_PREFACE 24

static_assert((sizeof(HTTP2_CLIENT_PREFACE) / (sizeof(HTTP2_CLIENT_PREFACE[0]))) - 1 ==
              SIZEOF_HTTP2_CLIENT_PREFACE);

#define HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE "\r\nSM\r\n\r\n"

#define SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE 8

static_assert((sizeof(HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE) /
               (sizeof(HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE[0]))) -
                  1 ==
              SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE);

NODISCARD Http2PrefaceStatus analyze_http2_preface(HttpRequestLine request_line,
                                                   ParseState* state) {

	if(request_line.protocol_version != HTTPProtocolVersion2) {
		return Http2PrefaceStatusErr;
	}

	if(request_line.method != HTTPRequestMethodPRI) {
		return Http2PrefaceStatusErr;
	}

	if(request_line.uri.type != ParsedURITypeAsterisk) {
		return Http2PrefaceStatusErr;
	}

	size_t remaining_size = state->data.size - state->cursor;

	if(remaining_size < SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE) {
		return Http2PrefaceStatusNotEnoughData;
	}

	if(memcmp(state->data.data, HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE,
	          SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE) == 0) {

		state->cursor += SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE;
		return Http2PrefaceStatusOk;
	}

	return Http2PrefaceStatusErr;
}
