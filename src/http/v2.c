

#include "./v2.h"
#include "generic/send.h"

TVEC_IMPLEMENT_VEC_TYPE(Http2Frame)

#define HTTP2_HEADER_SIZE 9

typedef struct {
	uint32_t length : 24; // size without this header
	uint8_t type;
	uint8_t flags;
	bool _reserved : 1;              // padding for alignment
	uint32_t stream_identifier : 31; // only 31 bits!
} Http2RawHeader;

NODISCARD static Http2RawHeader parse_http2_raw_header(const uint8_t* const header_data) {

	uint8_t type = header_data[3];

	uint8_t flags = header_data[4];

	// this is big endian!
	uint32_t length = (header_data[0] << 16) | (header_data[1] << 8) | (header_data[2]);

	// this is big endian!
	uint32_t stream_identifier = ((uint32_t)(header_data[5] & 0x7f) << 24) |
	                             ((uint32_t)header_data[6] << 16) |
	                             ((uint32_t)header_data[7] << 8) | ((uint32_t)header_data[8]);

	// MUST be ignored in receiving
	// bool reserved = ((stream_identifier_raw >> 31) & 1) == 1;

	Http2RawHeader result = (Http2RawHeader){
		.length = length,
		.type = type,
		.flags = flags,
		.stream_identifier = stream_identifier,
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
	Http2FrameFlagEndStream = 0x01,
	Http2FrameFlagEndHeaders = 0x04,
	Http2FrameFlagPadded = 0x08,
	Http2FrameFlagPriority = 0x20,
} Http2FrameFlag;

#define DEFAULT_SETTINGS_MAX_FRAME_SIZE (1 << 14) // 2^14

NODISCARD HTTP2State http2_default_state(void) {

	return (HTTP2State){
		.settings =
		    (Http2Settings){
		        .max_frame_size = DEFAULT_SETTINGS_MAX_FRAME_SIZE,
		    },
		.requests = TMAP_EMPTY(Http2PartialRequestMap),
	};
}

typedef struct {
	bool is_error;
	union {
		Http2Frame frame;
		const char* error;
	} data;
} Http2FrameResult;

NODISCARD static SizedBuffer serialize_http2_frame_header(const Http2RawHeader header) {

	SizedBuffer header_data = allocate_sized_buffer(HTTP2_HEADER_SIZE);

	if(header_data.data == NULL) {
		return header_data;
	}

	{
		size_t i = 0;

		uint8_t* data = (uint8_t*)header_data.data;

		/* Length: 24 bits, big endian */
		data[i++] = (header.length >> 16) & 0xff;
		data[i++] = (header.length >> 8) & 0xff;
		data[i++] = header.length & 0xff;

		data[i++] = header.type;

		data[i++] = header.flags;

		/* Stream-ID (reserved bit cleared) */
		size_t stream_id = header.stream_identifier & 0x7fffffff;
		data[i++] = (stream_id >> 24) & 0xff;
		data[i++] = (stream_id >> 16) & 0xff;
		data[i++] = (stream_id >> 8) & 0xff;
		data[i++] = stream_id & 0xff;

		assert(i == header_data.size && "implemented http2 frame header serialization incorrectly");
	}

	return header_data;
}

NODISCARD static int http2_send_raw_frame(const ConnectionDescriptor* const descriptor,
                                          const Http2RawHeader header, const SizedBuffer data) {

	// TODO: support padding

	if(data.size != header.length) {
		return -71;
	}

	SizedBuffer header_buffer = serialize_http2_frame_header(header);

	if(header_buffer.data == NULL) {
		return -72;
	}

	int result = send_sized_buffer_to_connection(descriptor, header_buffer);

	if(result < 0) {
		free_sized_buffer(header_buffer);
		return result;
	}

	result = send_sized_buffer_to_connection(descriptor, data);

	if(result < 0) {
		free_sized_buffer(header_buffer);
		return result;
	}

	return result;
}

#define HTTP2_FRAME_GOAWAY_BASE_SIZE ((32 + 32) / 8)

NODISCARD static int http2_send_goaway_frame(const ConnectionDescriptor* const descriptor,
                                             Http2GoawayFrame frame) {

	uint32_t length = frame.additional_debug_data.size + HTTP2_FRAME_GOAWAY_BASE_SIZE;

	Http2RawHeader header = {
		.length = length,
		.type = Http2FrameTypeGoaway,
		.flags = 0,
		.stream_identifier = 0,
	};

	SizedBuffer frame_as_data = allocate_sized_buffer(length);

	if(frame_as_data.data == NULL) {
		return -1;
	}

	{
		size_t i = 0;

		uint8_t* data = (uint8_t*)frame_as_data.data;

		/* Last-Stream-ID (reserved bit cleared) */
		size_t last_stream_id = frame.last_stream_id & 0x7fffffff;
		data[i++] = (last_stream_id >> 24) & 0xff;
		data[i++] = (last_stream_id >> 16) & 0xff;
		data[i++] = (last_stream_id >> 8) & 0xff;
		data[i++] = last_stream_id & 0xff;

		/* Error Code */
		data[i++] = (frame.error_code >> 24) & 0xff;
		data[i++] = (frame.error_code >> 16) & 0xff;
		data[i++] = (frame.error_code >> 8) & 0xff;
		data[i++] = frame.error_code & 0xff;

		/* Optional debug data */
		if(frame.additional_debug_data.size > 0) {
			memcpy((void*)(data + i), frame.additional_debug_data.data,
			       frame.additional_debug_data.size);
			i += frame.additional_debug_data.size;
		}

		assert(i == length && "implemented goaway serialization incorrectly");
	}

	const int result = http2_send_raw_frame(descriptor, header, frame_as_data);

	free_sized_buffer(frame_as_data);

	return result;
}

NODISCARD static int http2_close_stream_with_error(ConnectionDescriptor* const descriptor,
                                                   Http2ErrorCode error_code, const char* error) {

	SizedBuffer additional_debug_data =
	    error == NULL ? get_empty_sized_buffer() : sized_buffer_from_cstr(error);

	// TODO: set last_stream_id correctly
	Http2GoawayFrame frame = {
		.last_stream_id = 0,
		.error_code = error_code,
		.additional_debug_data = additional_debug_data,
	};

	int result = http2_send_goaway_frame(descriptor, frame);

	int result2 = close_connection_descriptor(descriptor);

	if(result2 < 0) {
		return result2;
	}

	return result;
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2DataFrameFlagEndStream = Http2FrameFlagEndStream,
	Http2DataFrameFlagPadded = Http2FrameFlagPadded,
	// all allowed flags or-ed together
	Http2DataFrameFlagsAllowed = Http2DataFrameFlagEndStream | Http2DataFrameFlagPadded
} Http2DataFrameFlag;

NODISCARD static Http2FrameResult parse_http2_data_frame(const HTTP2State* const state,
                                                         BufferedReader* const reader,
                                                         Http2RawHeader http2_raw_header) {

	if((http2_raw_header.flags & Http2DataFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid data frame flags";
		int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
		                                      Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	uint8_t padding_length = 0;
	size_t payload_length = http2_raw_header.length;

	if((http2_raw_header.flags & Http2DataFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const char* error = "not enough frame data fro padding length field(1 byte)";
			int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
			                                      Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame header";
			int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
			                                      Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.data;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const char* error = "padding length is greater than remaining length in frame";
			int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
			                                      Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame data";
		int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
		                                      Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = read_result.value.data;

	if(padding_length != 0) {
		BufferedReadResult read_result2 = buffered_reader_get_amount(reader, payload_length);

		if(read_result2.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the padding data";
			int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
			                                      Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result2.value.data;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const char* error = "padding bytes are not 0";
				int _ =
				    http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
				                                  Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
		}
	}

	Http2Frame frame = { .type = Http2FrameTypeData,
		                 .value = { .data = (Http2DataFrame){ .content = frame_data } } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

NODISCARD static Http2FrameResult parse_http2_frame(const HTTP2State* const state,
                                                    BufferedReader* const reader) {

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_HEADER_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
		                                      Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer header_data = read_result.value.data;

	const Http2RawHeader http2_raw_header = parse_http2_raw_header(header_data.data);

	if(http2_raw_header.length > state->settings.max_frame_size) {
		const char* error = "Header size too big";
		int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
		                                      Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(http2_raw_header.stream_identifier == 0 && http2_raw_header.type != Http2FrameTypeSettings) {
		const char* error = "Frame type doesn't allow stream id 0";
		int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
		                                      Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	switch(http2_raw_header.type) {
		case Http2FrameTypeData: {
			return parse_http2_data_frame(state, reader, http2_raw_header);
			break;
		}
		case Http2FrameTypeHeaders: {
			break;
		}
		case Http2FrameTypePriority: {
			break;
		}
		case Http2FrameTypeRstStream: {
			break;
		}
		case Http2FrameTypeSettings: {
			break;
		}
		case Http2FrameTypePushPromise: {
			break;
		}
		case Http2FrameTypePing: {
			break;
		}
		case Http2FrameTypeGoaway: {
			break;
		}
		case Http2FrameTypeWindowUpdate: {
			break;
		}
		case Http2FrameTypeContinuation: {
			break;
		}
		default: {
			const char* error = "Unrecognized frame type";
			int _ = http2_close_stream_with_error(buffered_reader_get_connection_descriptor(reader),
			                                      Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}
}

NODISCARD HttpRequestResult parse_http2_request(HTTP2State* const state,
                                                BufferedReader* const reader) {

	Http2Frames frames = TVEC_EMPTY(Http2Frame);

	// TODO: Support the SETTINGS_MAX_FRAME_SIZE setting, in some cases, the settings header
	// needs to be sent as first, or in h2c cases, it is required to supply it  via a http 1
	// header
	while(true) {
		Http2FrameResult frame_result = parse_http2_frame(state, reader);
		// process the frame, if possible, otherwise do it later (e.g. when the headers end)

		if(frame_result.is_error) {
			return (HttpRequestResult){ .is_error = true,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = frame_result.data.error ,}

				                                },
				                        } };
		}

		auto _ = TVEC_PUSH(Http2Frame, &frames, frame_result.data.frame);
		UNUSED(_);
	}
	//

	UNREACHABLE();
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
                                                   BufferedReader* reader) {

	if(request_line.protocol_version != HTTPProtocolVersion2) {
		return Http2PrefaceStatusErr;
	}

	if(request_line.method != HTTPRequestMethodPRI) {
		return Http2PrefaceStatusErr;
	}

	if(request_line.uri.type != ParsedURITypeAsterisk) {
		return Http2PrefaceStatusErr;
	}

	BufferedReadResult res =
	    buffered_reader_get_amount(reader, SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE);

	if(res.type != BufferedReadResultTypeOk) {
		return Http2PrefaceStatusNotEnoughData;
	}

	if(sized_buffer_cmp_with_data(res.value.data, HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE,
	                              SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE) == 0) {

		return Http2PrefaceStatusOk;
	}

	return Http2PrefaceStatusErr;
}
