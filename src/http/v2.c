#include "./v2.h"
#include "generic/send.h"
#include "generic/serialize.h"

TVEC_IMPLEMENT_VEC_TYPE(Http2Frame)

TVEC_IMPLEMENT_VEC_TYPE(Http2SettingSingleValue)

#define HTTP2_HEADER_SIZE 9

typedef struct {
	uint32_t length : 24; // size without this header
	uint8_t type;
	uint8_t flags;
	Http2Identifier stream_identifier;
} Http2RawHeader;

NODISCARD static Http2Identifier deserialize_identifier(const uint8_t* const bytes) {

	const uint32_t identifier = deserialize_u32_be_to_host(bytes) & 0x7fffffffULL;

	return (Http2Identifier){ .identifier = identifier };
}

NODISCARD static SerializeResult32 serialize_identifier(const Http2Identifier identifier) {

	const SerializeResult32 result = serialize_u32_host_to_be(identifier.identifier & 0x7fffffff);

	return result;
}

NODISCARD static Http2RawHeader parse_http2_raw_header(const uint8_t* const header_data) {

	uint8_t type = header_data[3];

	uint8_t flags = header_data[4];

	uint32_t length = deserialize_u32_be_to_host(header_data) & 0x00ffffffULL;

	Http2Identifier stream_identifier = deserialize_identifier(header_data + 5);

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

#define UINT32_T_MAX 0xFFFFFFFF

#define DEFAULT_HEADER_TABLE_SIZE 4096
#define DEFAULT_MAX_CONCURRENT_STREAMS UINT32_T_MAX
#define DEFAULT_INITIAL_WINDOW_SIZE 65535         // 2^16-1 (65,535)
#define DEFAULT_SETTINGS_MAX_FRAME_SIZE (1 << 14) // 2^14
#define DEFAULT_MAX_HEADER_LIST_SIZE UINT32_T_MAX

NODISCARD static Http2Settings http2_default_settings(void) {
	return (Http2Settings){
		.header_table_size = DEFAULT_HEADER_TABLE_SIZE,
		.enable_push = true,
		.max_concurrent_streams = DEFAULT_MAX_CONCURRENT_STREAMS,
		.initial_window_size = DEFAULT_INITIAL_WINDOW_SIZE,
		.max_frame_size = DEFAULT_SETTINGS_MAX_FRAME_SIZE,
		.max_header_list_size = DEFAULT_MAX_HEADER_LIST_SIZE,
	};
}

NODISCARD HTTP2Context http2_default_context(void) {

	return (HTTP2Context){ .settings = http2_default_settings(),
		                   .streams = TMAP_EMPTY(Http2StreamMap),
		                   .frames = TVEC_EMPTY(Http2Frame),
		                   .state = (Http2State){
		                       .last_stream_id = 0,
		                   } };
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

		const SerializeResult32 length_res = serialize_u32_host_to_be(header.length);

		data[i++] = length_res.bytes[1];
		data[i++] = length_res.bytes[2];
		data[i++] = length_res.bytes[3];

		data[i++] = header.type;

		data[i++] = header.flags;

		const SerializeResult32 stream_id_res = serialize_identifier(header.stream_identifier);

		data[i++] = stream_id_res.bytes[0];
		data[i++] = stream_id_res.bytes[1];
		data[i++] = stream_id_res.bytes[2];
		data[i++] = stream_id_res.bytes[3];

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

	free_sized_buffer(header_buffer);

	return result;
}

#define HTTP2_FRAME_GOAWAY_BASE_SIZE ((32 + 32) / 8)

#define HTTP2_CONNECTION_STREAM_IDENTIFIER ((Http2Identifier){ .identifier = 0 })

NODISCARD static int http2_send_goaway_frame(const ConnectionDescriptor* const descriptor,
                                             Http2GoawayFrame frame) {

	uint32_t length = frame.additional_debug_data.size + HTTP2_FRAME_GOAWAY_BASE_SIZE;

	Http2RawHeader header = {
		.length = length,
		.type = Http2FrameTypeGoaway,
		.flags = 0,
		.stream_identifier = HTTP2_CONNECTION_STREAM_IDENTIFIER,
	};

	SizedBuffer frame_as_data = allocate_sized_buffer(length);

	if(frame_as_data.data == NULL) {
		return -1;
	}

	{
		size_t i = 0;

		uint8_t* data = (uint8_t*)frame_as_data.data;

		const SerializeResult32 last_stream_id_res = serialize_identifier(frame.last_stream_id);

		data[i++] = last_stream_id_res.bytes[0];
		data[i++] = last_stream_id_res.bytes[1];
		data[i++] = last_stream_id_res.bytes[2];
		data[i++] = last_stream_id_res.bytes[3];

		const SerializeResult32 error_code_res = serialize_u32_host_to_be(frame.error_code);

		data[i++] = error_code_res.bytes[0];
		data[i++] = error_code_res.bytes[1];
		data[i++] = error_code_res.bytes[2];
		data[i++] = error_code_res.bytes[3];

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

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2SettingsFrameFlagAck = 0x1,
	// all allowed flags or-ed together
	Http2SettingsFrameFlagsAllowed = Http2SettingsFrameFlagAck
} Http2SettingsFrameFlag;

NODISCARD static int http2_send_settings_frame(const ConnectionDescriptor* const descriptor,
                                               Http2SettingsFrame frame) {

	uint32_t length = TVEC_LENGTH(Http2SettingSingleValue, frame.entries);

	uint8_t flags = frame.ack ? Http2SettingsFrameFlagAck : 0;

	Http2RawHeader header = {
		.length = length,
		.type = Http2FrameTypeSettings,
		.flags = flags,
		.stream_identifier = HTTP2_CONNECTION_STREAM_IDENTIFIER,
	};

	if(frame.ack && length != 0) {
		return -19;
	}

	SizedBuffer frame_as_data = allocate_sized_buffer(length);

	if(frame_as_data.data == NULL) {
		return -1;
	}

	{

		uint8_t* data = (uint8_t*)frame_as_data.data;

		for(size_t i = 0; i < TVEC_LENGTH(Http2SettingSingleValue, frame.entries); ++i) {
			const Http2SettingSingleValue entry =
			    TVEC_AT(Http2SettingSingleValue, frame.entries, i);

			const SerializeResult16 identifier_res = serialize_u16_host_to_be(entry.identifier);

			data[i] = identifier_res.bytes[0];
			data[i + 1] = identifier_res.bytes[1];

			const SerializeResult32 value_res = serialize_u32_host_to_be(entry.value);

			data[i + 2] = value_res.bytes[0];
			data[i + 3] = value_res.bytes[1];
			data[i + 4] = value_res.bytes[2];
			data[i + 5] = value_res.bytes[3];
		}
	}

	const int result = http2_send_raw_frame(descriptor, header, frame_as_data);

	free_sized_buffer(frame_as_data);

	return result;
}

NODISCARD int http2_send_stream_error(const ConnectionDescriptor* const descriptor,
                                      Http2ErrorCode error_code, const char* error) {

	SizedBuffer debug_data =
	    error == NULL ? get_empty_sized_buffer() : sized_buffer_from_cstr(error);

	return http2_send_stream_error_with_data(descriptor, error_code, debug_data);
}

NODISCARD int http2_send_stream_error_with_data(const ConnectionDescriptor* const descriptor,
                                                Http2ErrorCode error_code, SizedBuffer debug_data) {

	// TODO: set last_stream_id correctly
	Http2GoawayFrame frame = {
		.last_stream_id = HTTP2_CONNECTION_STREAM_IDENTIFIER,
		.error_code = error_code,
		.additional_debug_data = debug_data,
	};

	return http2_send_goaway_frame(descriptor, frame);
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

NODISCARD static Http2FrameResult parse_http2_data_frame(BufferedReader* const reader,
                                                         Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const char* error = "Data Frame doesn't allow stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2DataFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid data frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	uint8_t padding_length = 0;
	size_t payload_length = http2_raw_header.length;

	if((http2_raw_header.flags & Http2DataFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const char* error = "not enough frame data for padding length field(1 byte)";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame header";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.data;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const char* error = "padding length is greater than remaining length in frame";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(padding_length != 0) {
		BufferedReadResult read_result2 = buffered_reader_get_amount(reader, payload_length);

		if(read_result2.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the padding data";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result2.value.data;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const char* error = "padding bytes are not 0";
				int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
				                                Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
		}
	}

	SizedBuffer frame_data = read_result.value.data;

	if(frame_data.size == 0) {
		frame_data.data = NULL;
	} else {
		frame_data = sized_buffer_dup(frame_data);

		if(frame_data.data == NULL) {
			const char* error = "Failed allocate frame data content buffer";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	const Http2DataFrame data_frame = {
		.content = frame_data,
		.is_end = (http2_raw_header.flags & Http2DataFrameFlagEndStream) != 0,
		.identifier = http2_raw_header.stream_identifier,
	};

	Http2Frame frame = { .type = Http2FrameTypeData, .value = { .data = data_frame } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

#define HTTP2_DEPENDENCY_INFO_SIZE ((32 + 8) / 8)

NODISCARD static Http2FrameDependency
get_http2_dependency_info_from_raw_data(const SizedBuffer raw_data) {
	uint8_t* dependency_data_raw = (uint8_t*)raw_data.data;

	uint32_t stream_dependency_identifier_raw = deserialize_u32_be_to_host(dependency_data_raw);

	uint32_t dependency_identifier = stream_dependency_identifier_raw & 0x7FFFFFFF;

	bool exclusive = (stream_dependency_identifier_raw >> 31) != 0;

	uint8_t weight = dependency_data_raw[4];

	Http2FrameDependency dependency = {
		.exclusive = exclusive,
		.dependency_identifier = dependency_identifier,
		.weight = weight,
	};

	return dependency;
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2HeadersFrameFlagEndStream = Http2FrameFlagEndStream,
	Http2HeadersFrameFlagEndHeaders = Http2FrameFlagEndHeaders,
	Http2HeadersFrameFlagPadded = Http2FrameFlagPadded,
	Http2HeadersFrameFlagPriority = Http2FrameFlagPriority,
	// all allowed flags or-ed together
	Http2HeadersFrameFlagsAllowed = Http2HeadersFrameFlagEndStream |
	                                Http2HeadersFrameFlagEndHeaders | Http2HeadersFrameFlagPadded |
	                                Http2HeadersFrameFlagPriority
} Http2HeadersFrameFlag;

NODISCARD static Http2FrameResult parse_http2_headers_frame(BufferedReader* const reader,
                                                            Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const char* error = "Headers Frame doesn't allow stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2HeadersFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid headers frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	size_t payload_length = http2_raw_header.length;

	uint8_t padding_length = 0;

	if((http2_raw_header.flags & Http2DataFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const char* error = "not enough frame data for padding length field(1 byte)";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame header";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.data;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const char* error = "padding length is greater than remaining length in frame";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	Http2FrameDependencyOptional dependency_opt = {
		.has_dependency = (http2_raw_header.flags & Http2HeadersFrameFlagPadded) != 0,
		.dependency = { .exclusive = false, .dependency_identifier = 0, .weight = 0 }
	};

	if(dependency_opt.has_dependency) {
		if(payload_length < HTTP2_DEPENDENCY_INFO_SIZE) {
			const char* error = "not enough frame data for priority info";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - HTTP2_DEPENDENCY_INFO_SIZE;

		BufferedReadResult read_result =
		    buffered_reader_get_amount(reader, HTTP2_DEPENDENCY_INFO_SIZE);

		if(read_result.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame header";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer dependency_data = read_result.value.data;

		Http2FrameDependency dependency = get_http2_dependency_info_from_raw_data(dependency_data);

		dependency_opt.dependency = dependency;
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(padding_length != 0) {
		BufferedReadResult read_result2 = buffered_reader_get_amount(reader, payload_length);

		if(read_result2.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the padding data";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result2.value.data;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const char* error = "padding bytes are not 0";
				int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
				                                Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
		}
	}

	SizedBuffer block_fragment = read_result.value.data;

	if(block_fragment.size == 0) {
		block_fragment.data = NULL;
	} else {
		block_fragment = sized_buffer_dup(block_fragment);

		if(block_fragment.data == NULL) {
			const char* error = "Failed allocate headers block fragment buffer";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	Http2HeadersFrame headers_frame = {
		.dependency_opt = dependency_opt,
		.block_fragment = block_fragment,
		.identifier = http2_raw_header.stream_identifier,
		.end_stream = (http2_raw_header.flags & Http2HeadersFrameFlagEndStream) != 0,
		.end_headers = (http2_raw_header.flags & Http2HeadersFrameFlagEndHeaders) != 0,
	};

	Http2Frame frame = { .type = Http2FrameTypeHeaders, .value = { .headers = headers_frame } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	// all allowed flags or-ed together
	Http2PriorityFrameFlagsAllowed = 0
} Http2PriorityFrameFlag;

NODISCARD static Http2FrameResult parse_http2_priority_frame(BufferedReader* const reader,
                                                             Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const char* error = "Priority Frame doesn't allow stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2PriorityFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid priority frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	if(payload_length != HTTP2_DEPENDENCY_INFO_SIZE) {
		const char* error = "not enough frame data for priority info";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_DEPENDENCY_INFO_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer dependency_data = read_result.value.data;

	Http2FrameDependency dependency = get_http2_dependency_info_from_raw_data(dependency_data);

	Http2PriorityFrame priority_frame = {
		.dependency = dependency,
		.identifier = http2_raw_header.stream_identifier,
	};

	Http2Frame frame = {
		.type = Http2FrameTypePriority,
		.value = { .priority = priority_frame },
	};

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	// all allowed flags or-ed together
	Http2RestStreamFrameFlagsAllowed = 0
} Http2RestStreamFrameFlag;

#define HTTP2_RST_STREAM_SIZE (32 / 8)

NODISCARD static Http2FrameResult parse_http2_rst_stream_frame(BufferedReader* const reader,
                                                               Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const char* error = "Rst stream Frame doesn't allow stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2RestStreamFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid rst stream frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	if(payload_length != HTTP2_RST_STREAM_SIZE) {
		const char* error = "not enough frame data for rst stream data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_RST_STREAM_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer dependency_data = read_result.value.data;

	const uint32_t error_code = deserialize_u32_be_to_host((uint8_t*)dependency_data.data);

	Http2RstStreamFrame rst_stream_frame = {
		.error_code = error_code,
		.identifier = http2_raw_header.stream_identifier,
	};

	Http2Frame frame = {
		.type = Http2FrameTypeRstStream,
		.value = { .rst_stream = rst_stream_frame },
	};

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

#define MAX_FLOW_CONTROL_WINDOW_SIZE ((1ULL << 31ULL) - 1) // 2^31-1

#define MAX_MAX_FRAME_SIZE 16777215ULL // 2^24-1 or 16,777,215

NODISCARD static Http2FrameResult parse_http2_settings_frame(BufferedReader* const reader,
                                                             Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const char* error = "Settings Frame only allows stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2SettingsFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid settings frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	Http2SettingsFrame settings_frame = {
		.ack = (http2_raw_header.flags & Http2SettingsFrameFlagAck) != 0,
		.entries = TVEC_EMPTY(Http2SettingSingleValue),
	};

	if(settings_frame.ack) {
		if(http2_raw_header.length != 0) {
			const char* error = "ack in settings frame with a non zero payload is invalid";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeFrameSizeError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		Http2Frame frame = { .type = Http2FrameTypeSettings,
			                 .value = {
			                     .settings = settings_frame,
			                 } };
		return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
	}

	if(http2_raw_header.length % 6 != 0) {
		const char* error = "invalid settings frame length, not a multiple of 6";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(http2_raw_header.length > 0) {

		BufferedReadResult read_result =
		    buffered_reader_get_amount(reader, http2_raw_header.length);

		if(read_result.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame data";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer frame_data = read_result.value.data;

		uint8_t* data = (uint8_t*)frame_data.data;

		for(size_t i = 0; i < http2_raw_header.length; i += 6) {
			uint16_t identifier = deserialize_u16_be_to_host(data + i);

			uint32_t value = deserialize_u32_be_to_host(data + i + 2);

			switch(identifier) {
				case Http2SettingsFrameIdentifierHeaderTableSize: {
					break;
				}
				case Http2SettingsFrameIdentifierEnablePush: {
					if(value != 0 && value != 1) {
						const char* error = "Invalid SETTINGS_ENABLE_PUSH settings value";
						int _ = http2_send_stream_error(
						    buffered_reader_get_connection_descriptor(reader),
						    Http2ErrorCodeProtocolError, error);
						UNUSED(_);
						return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
					}
					break;
				}
				case Http2SettingsFrameIdentifierMaxConcurrentStreams: {
					break;
				}
				case Http2SettingsFrameIdentifierInitialWindowSize: {

					if(value > MAX_FLOW_CONTROL_WINDOW_SIZE) {
						const char* error = "Invalid SETTINGS_INITIAL_WINDOW_SIZE settings value";
						int _ = http2_send_stream_error(
						    buffered_reader_get_connection_descriptor(reader),
						    Http2ErrorCodeFlowControlError, error);
						UNUSED(_);
						return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
					}
					break;
				}
				case Http2SettingsFrameIdentifierMaxFrameSize: {
					if(value > MAX_MAX_FRAME_SIZE + 1 || value < DEFAULT_SETTINGS_MAX_FRAME_SIZE) {
						const char* error = "Invalid SETTINGS_MAX_FRAME_SIZE settings value";
						int _ = http2_send_stream_error(
						    buffered_reader_get_connection_descriptor(reader),
						    Http2ErrorCodeProtocolError, error);
						UNUSED(_);
						return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
					}
					break;
				}
				case Http2SettingsFrameIdentifierMaxHeaderListSize: {

					break;
				}
				default: {
					// An endpoint that receives a SETTINGS frame with any unknown or
					// unsupported identifier MUST ignore that setting.
					continue;
				}
			}

			Http2SettingSingleValue entry = { .identifier = identifier, .value = value };

			auto _ = TVEC_PUSH(Http2SettingSingleValue, &settings_frame.entries, entry);
			UNUSED(_);
		}
	}

	Http2Frame frame = { .type = Http2FrameTypeSettings,
		                 .value = {
		                     .settings = settings_frame,
		                 } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2PushPromiseFrameFlagEndHeaders = Http2FrameFlagEndHeaders,
	Http2PushPromiseFrameFlagPadded = Http2FrameFlagPadded,
	// all allowed flags or-ed together
	Http2PushPromiseFrameFlagsAllowed = Http2PushPromiseFrameFlagEndHeaders |
	                                    Http2PushPromiseFrameFlagPadded
} Http2PushPromiseFrameFlag;

NODISCARD static Http2FrameResult parse_http2_push_promise_frame(const Http2Settings settings,
                                                                 BufferedReader* const reader,
                                                                 Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const char* error = "Push promise Frame only allows stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2PushPromiseFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid push promise frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(!settings.enable_push) {
		const char* error = "push promise frames are disabled per settings";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	size_t payload_length = http2_raw_header.length;

	uint8_t padding_length = 0;

	if((http2_raw_header.flags & Http2PushPromiseFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const char* error = "not enough frame data for padding length field(1 byte)";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame header";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.data;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const char* error = "padding length is greater than remaining length in frame";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	if(payload_length < 4) {
		const char* error = "not enough frame data for promised stream identifier field(4 bytes)";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	payload_length = payload_length - 4;

	BufferedReadResult read_result2 = buffered_reader_get_amount(reader, 1);

	if(read_result2.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer promised_stream_identifier_data = read_result2.value.data;

	const Http2Identifier promised_stream_identifier =
	    deserialize_identifier((uint8_t*)promised_stream_identifier_data.data);

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(padding_length != 0) {
		BufferedReadResult read_result3 = buffered_reader_get_amount(reader, payload_length);

		if(read_result3.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the padding data";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result3.value.data;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const char* error = "padding bytes are not 0";
				int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
				                                Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
		}
	}

	SizedBuffer block_fragment = read_result.value.data;

	if(block_fragment.size == 0) {
		block_fragment.data = NULL;
	} else {
		block_fragment = sized_buffer_dup(block_fragment);

		if(block_fragment.data == NULL) {
			const char* error = "Failed allocate headers block fragment buffer";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	Http2PushPromiseFrame push_promise_frame = {
		.promised_stream_identifier = promised_stream_identifier,
		.block_fragment = block_fragment,
		.identifier = http2_raw_header.stream_identifier,
		.end_headers = (http2_raw_header.flags & Http2PushPromiseFrameFlagEndHeaders) != 0,
	};

	Http2Frame frame = { .type = Http2FrameTypePushPromise,
		                 .value = {
		                     .push_promise = push_promise_frame,
		                 } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

#define HTTP2_PING_FRAME_SIZE (8)

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2PingFrameFlagAck = 0x1,
	// all allowed flags or-ed together
	Http2PingFrameFlagsAllowed = Http2PingFrameFlagAck
} Http2PingFrameFlag;

NODISCARD static Http2FrameResult parse_http2_ping_frame(BufferedReader* const reader,
                                                         Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const char* error = "The ping Frame only allows stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2PingFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid ping frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	if(payload_length != HTTP2_PING_FRAME_SIZE) {
		const char* error = "not enough frame data for ping data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_PING_FRAME_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer opaque_data = read_result.value.data;

	opaque_data = sized_buffer_dup(opaque_data);

	if(opaque_data.data == NULL) {
		const char* error = "Failed allocate frame data content buffer";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	Http2PingFrame ping_frame = {
		.ack = (http2_raw_header.flags & Http2PingFrameFlagAck) != 0,
		.opaque_data = opaque_data,
	};

	Http2Frame frame = {
		.type = Http2FrameTypePing,
		.value = { .ping = ping_frame },
	};

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

#define BASE_HTTP2_GOAWAY_FRAME_SIZE ((32 + 32) / 8)

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	// all allowed flags or-ed together
	Http2GoawayFrameFlagsAllowed = 0x00
} Http2GoawayFrameFlag;

NODISCARD static Http2FrameResult parse_http2_goaway_frame(BufferedReader* const reader,
                                                           Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const char* error = "The goaway Frame only allows stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2GoawayFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid goaway frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(http2_raw_header.length < BASE_HTTP2_GOAWAY_FRAME_SIZE) {
		const char* error = "invalid goaway frame length, not enough data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t additional_data_size = http2_raw_header.length - BASE_HTTP2_GOAWAY_FRAME_SIZE;

	BufferedReadResult read_result =
	    buffered_reader_get_amount(reader, BASE_HTTP2_GOAWAY_FRAME_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = read_result.value.data;

	uint8_t* data = (uint8_t*)frame_data.data;

	const Http2Identifier last_stream_id = deserialize_identifier(data);

	uint32_t error_code = deserialize_u32_be_to_host(data + sizeof(last_stream_id));

	SizedBuffer additional_debug_data = { .data = NULL, .size = 0 };

	if(additional_data_size > 0) {

		BufferedReadResult read_result2 = buffered_reader_get_amount(reader, additional_data_size);

		if(read_result2.type != BufferedReadResultTypeOk) {
			const char* error = "Failed to read enough data for the frame data";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		additional_debug_data = sized_buffer_dup(read_result2.value.data);

		if(additional_debug_data.data == NULL) {
			const char* error = "Failed allocate frame data content buffer";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	Http2GoawayFrame goaway_frame = {
		.last_stream_id = last_stream_id,
		.error_code = error_code,
		.additional_debug_data = additional_debug_data,
	};

	Http2Frame frame = { .type = Http2FrameTypeGoaway,
		                 .value = {
		                     .goaway = goaway_frame,
		                 } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

#define HTTP2_WINDOW_UPDATE_FRAME_SIZE (4)

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	// all allowed flags or-ed together
	Http2WindowUpdateFrameFlagsAllowed = 0x00
} Http2WindowUpdateFrameFlag;

NODISCARD static Http2FrameResult parse_http2_window_update_frame(BufferedReader* const reader,
                                                                  Http2RawHeader http2_raw_header) {

	if((http2_raw_header.flags & Http2WindowUpdateFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid windows update frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(http2_raw_header.length != HTTP2_WINDOW_UPDATE_FRAME_SIZE) {
		const char* error = "invalid window update frame length, not enough data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result =
	    buffered_reader_get_amount(reader, HTTP2_WINDOW_UPDATE_FRAME_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = read_result.value.data;

	uint8_t* data = (uint8_t*)frame_data.data;

	const uint32_t window_size_increment = deserialize_u32_be_to_host(data) & 0x7fffffffULL;

	Http2WindowUpdateFrame window_update_frame = {
		.window_size_increment = window_size_increment,
		.identifier = http2_raw_header.stream_identifier,
	};

	Http2Frame frame = { .type = Http2FrameTypeWindowUpdate,
		                 .value = {
		                     .window_update = window_update_frame,
		                 } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2ContinuationFrameFlagEndHeaders = Http2FrameFlagEndHeaders,
	// all allowed flags or-ed together
	Http2ContinuationFrameFlagsAllowed = Http2ContinuationFrameFlagEndHeaders
} Http2ContinuationFrameFlag;

NODISCARD static Http2FrameResult parse_http2_continuation_frame(BufferedReader* const reader,
                                                                 Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const char* error = "Continuation Frame doesn't allow stream id 0";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2ContinuationFrameFlagsAllowed) != http2_raw_header.flags) {
		const char* error = "invalid continuation frame flags";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame data";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer block_fragment = read_result.value.data;

	if(block_fragment.size == 0) {
		block_fragment.data = NULL;
	} else {
		block_fragment = sized_buffer_dup(block_fragment);

		if(block_fragment.data == NULL) {
			const char* error = "Failed allocate headers block fragment buffer";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	Http2ContinuationFrame continuation_frame = {
		.block_fragment = block_fragment,
		.identifier = http2_raw_header.stream_identifier,
		.end_headers = (http2_raw_header.flags & Http2ContinuationFrameFlagEndHeaders) != 0,
	};

	Http2Frame frame = { .type = Http2FrameTypeContinuation,
		                 .value = { .continuation = continuation_frame } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

NODISCARD static Http2FrameResult parse_http2_frame(const HTTP2Context* const context,
                                                    BufferedReader* const reader) {

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_HEADER_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const char* error = "Failed to read enough data for the frame header";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer header_data = read_result.value.data;

	const Http2RawHeader http2_raw_header = parse_http2_raw_header(header_data.data);

	if(http2_raw_header.length > context->settings.max_frame_size) {
		const char* error = "Header size too big";
		int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	switch(http2_raw_header.type) {
		case Http2FrameTypeData: {
			return parse_http2_data_frame(reader, http2_raw_header);
		}
		case Http2FrameTypeHeaders: {
			return parse_http2_headers_frame(reader, http2_raw_header);
		}
		case Http2FrameTypePriority: {
			return parse_http2_priority_frame(reader, http2_raw_header);
		}
		case Http2FrameTypeRstStream: {
			return parse_http2_rst_stream_frame(reader, http2_raw_header);
		}
		case Http2FrameTypeSettings: {
			return parse_http2_settings_frame(reader, http2_raw_header);
		}
		case Http2FrameTypePushPromise: {
			return parse_http2_push_promise_frame(context->settings, reader, http2_raw_header);
		}
		case Http2FrameTypePing: {
			return parse_http2_ping_frame(reader, http2_raw_header);
		}
		case Http2FrameTypeGoaway: {
			return parse_http2_goaway_frame(reader, http2_raw_header);
		}
		case Http2FrameTypeWindowUpdate: {
			return parse_http2_window_update_frame(reader, http2_raw_header);
		}
		case Http2FrameTypeContinuation: {
			return parse_http2_continuation_frame(reader, http2_raw_header);
		}
		default: {
			const char* error = "Unrecognized frame type";
			int _ = http2_send_stream_error(buffered_reader_get_connection_descriptor(reader),
			                                Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}
}

static void http2_apply_settings_frame(Http2Settings* const settings,
                                       const Http2SettingsFrame frame) {

	for(size_t i = 0; i < TVEC_LENGTH(Http2SettingSingleValue, frame.entries); ++i) {
		const Http2SettingSingleValue entry = TVEC_AT(Http2SettingSingleValue, frame.entries, i);

		switch(entry.identifier) {
			case Http2SettingsFrameIdentifierHeaderTableSize: {
				settings->header_table_size = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierEnablePush: {
				settings->enable_push = entry.value != 0;
				break;
			}
			case Http2SettingsFrameIdentifierMaxConcurrentStreams: {
				settings->max_concurrent_streams = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierInitialWindowSize: {
				settings->initial_window_size = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierMaxFrameSize: {
				settings->max_frame_size = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierMaxHeaderListSize: {
				settings->max_header_list_size = entry.value;
				break;
			}
			default: {
				break;
			}
		}
	}
}

static void free_http2_data_frame(Http2DataFrame frame) {
	free_sized_buffer(frame.content);
}

static void free_http2_headers_frame(Http2HeadersFrame frame) {
	free_sized_buffer(frame.block_fragment);
}

static inline void free_http2_priority_frame(Http2PriorityFrame frame) {
	// noop
	UNUSED(frame);
}

static inline void free_http2_rst_stream_frame(Http2RstStreamFrame frame) {
	// noop
	UNUSED(frame);
}

static void free_http2_settings_frame(Http2SettingsFrame frame) {
	TVEC_FREE(Http2SettingSingleValue, &frame.entries);
}

static void free_http2_push_promise_frame(Http2PushPromiseFrame frame) {
	free_sized_buffer(frame.block_fragment);
}

static void free_http2_ping_frame(Http2PingFrame frame) {
	free_sized_buffer(frame.opaque_data);
}

static void free_http2_goaway_frame(Http2GoawayFrame frame) {
	if(frame.additional_debug_data.size > 0) {
		free_sized_buffer(frame.additional_debug_data);
	}
}

static inline void free_http2_window_update_frame(Http2WindowUpdateFrame frame) {
	// noop
	UNUSED(frame);
}

static void free_http2_continuation_frame(Http2ContinuationFrame frame) {
	free_sized_buffer(frame.block_fragment);
}

static void free_http2_frame(Http2Frame frame) {

	switch(frame.type) {
		case Http2FrameTypeData: {
			free_http2_data_frame(frame.value.data);
			break;
		}
		case Http2FrameTypeHeaders: {
			free_http2_headers_frame(frame.value.headers);
			break;
		}
		case Http2FrameTypePriority: {
			free_http2_priority_frame(frame.value.priority);
			break;
		}
		case Http2FrameTypeRstStream: {
			free_http2_rst_stream_frame(frame.value.rst_stream);
			break;
		}
		case Http2FrameTypeSettings: {
			free_http2_settings_frame(frame.value.settings);
			break;
		}
		case Http2FrameTypePushPromise: {
			free_http2_push_promise_frame(frame.value.push_promise);
			break;
		}
		case Http2FrameTypePing: {
			free_http2_ping_frame(frame.value.ping);
			break;
		}
		case Http2FrameTypeGoaway: {
			free_http2_goaway_frame(frame.value.goaway);
			break;
		}
		case Http2FrameTypeWindowUpdate: {
			free_http2_window_update_frame(frame.value.window_update);
			break;
		}
		case Http2FrameTypeContinuation: {
			free_http2_continuation_frame(frame.value.continuation);
			break;
		}
		default: {
			break;
		}
	}
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2ProcessFrameResultTypeOk = 0,
	Http2ProcessFrameResultTypeNewFinishedRequest,
	Http2ProcessFrameResultTypeError
} Http2ProcessFrameResultType;

typedef struct {
	bool _unused;
} Http2ProcessFrameFinishedRequest;

typedef struct {
	Http2ProcessFrameResultType type;
	union {
		const char* error;
		Http2ProcessFrameFinishedRequest request;
	} value;
} Http2ProcessFrameResult;

NODISCARD static Http2ProcessFrameResult
process_http2_frame(HTTP2Context* const context, const Http2Frame frame,
                    ConnectionDescriptor* const descriptor) {
	// TODO: http2 state management, first frame must be settings frame etc.

	// TODO: handle stream states: https://datatracker.ietf.org/doc/html/rfc7540#section-5.1

	switch(frame.type) {
		case Http2FrameTypeData: {
			// TODO: process
			const Http2DataFrame data_frame = frame.value.data;
			UNUSED(data_frame);
			break;
		}
		case Http2FrameTypeHeaders: {
			// TODO: process
			const Http2HeadersFrame headers_frame = frame.value.headers;
			UNUSED(headers_frame);
			break;
		}
		case Http2FrameTypePriority: {
			// TODO: process
			const Http2PriorityFrame priority_frame = frame.value.priority;
			UNUSED(priority_frame);
			break;
		}
		case Http2FrameTypeRstStream: {
			// TODO: process
			const Http2RstStreamFrame rst_stream_frame = frame.value.rst_stream;
			UNUSED(rst_stream_frame);
			break;
		}
		case Http2FrameTypeSettings: {
			const Http2SettingsFrame settings_frame = frame.value.settings;

			if(!settings_frame.ack) {
				http2_apply_settings_frame(&(context->settings), settings_frame);
				Http2SettingsFrame frame_to_send = {
					.ack = true,
					.entries = TVEC_EMPTY(Http2SettingSingleValue),
				};
				int _ = http2_send_settings_frame(descriptor, frame_to_send);
				UNUSED(_);
			}
			break;
		}
		case Http2FrameTypePushPromise: {
			// TODO: process
			const Http2PushPromiseFrame push_promise_frame = frame.value.push_promise;
			UNUSED(push_promise_frame);
			break;
		}
		case Http2FrameTypePing: {
			// TODO: process
			const Http2PingFrame ping_frame = frame.value.ping;
			UNUSED(ping_frame);
			break;
		}
		case Http2FrameTypeGoaway: {
			// TODO: process
			const Http2GoawayFrame goaway_frame = frame.value.goaway;
			UNUSED(goaway_frame);
			break;
		}
		case Http2FrameTypeWindowUpdate: {
			// TODO: process
			const Http2WindowUpdateFrame window_update_frame = frame.value.window_update;
			UNUSED(window_update_frame);
			break;
		}
		case Http2FrameTypeContinuation: {
			// TODO: process
			const Http2ContinuationFrame continuation_frame = frame.value.continuation;
			UNUSED(continuation_frame);
			break;
		}
		default: {
			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
				                              .value = {
				                                  .error = "unkown frame type to process",
				                              } };
		}
	}

	free_http2_frame(frame);

	// TODO: if we have a request ready, return it!

	return (Http2ProcessFrameResult){
		.type = Http2ProcessFrameResultTypeOk,
	};
}

NODISCARD HttpRequestResult parse_http2_request(HTTP2Context* const context,
                                                BufferedReader* const reader) {

	while(true) {
		Http2FrameResult frame_result = parse_http2_frame(context, reader);
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

		const Http2Frame frame = frame_result.data.frame;

		// after the parsing of the frame, we can discard that data
		buffered_reader_invalidate_old_data(reader);

		const Http2ProcessFrameResult process_result =
		    process_http2_frame(context, frame, buffered_reader_get_connection_descriptor(reader));

		// TODO: if we have a request ready, return it!

		switch(process_result.type) {
			case Http2ProcessFrameResultTypeError: {
				return (HttpRequestResult){ .is_error = true,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = process_result.value.error ,}

				                                },
				                        } };
			}
			case Http2ProcessFrameResultTypeNewFinishedRequest: {
				// TODO
				break;
			}
			case Http2ProcessFrameResultTypeOk: {
				break;
			}
			default: {
				return (HttpRequestResult){ .is_error = true,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = "invalid process frame result type" ,}

				                                },
				                        } };
			}
		}
	}
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

NODISCARD Http2StartResult http2_send_and_receive_preface(HTTP2Context* const context,
                                                          BufferedReader* const reader) {

	// TODO(Totto): select the best settings, we should send
	Http2SettingsFrame frame_to_send = {
		.ack = false,
		.entries = TVEC_EMPTY(Http2SettingSingleValue),
	};

	int result =
	    http2_send_settings_frame(buffered_reader_get_connection_descriptor(reader), frame_to_send);

	if(result < 0) {
		return (Http2StartResult){
			.is_error = true,
			.value = { .error = "error in sending settings frame (server preface)" }
		};
	}

	Http2FrameResult frame_result = parse_http2_frame(context, reader);

	if(frame_result.is_error) {
		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = frame_result.data.error } };
	}

	const Http2Frame frame = frame_result.data.frame;

	if(frame.type != Http2FrameTypeSettings) {
		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = "first frame has to be a settings frame" } };
	}

	// after the parsing of the frame, we can discard that data
	buffered_reader_invalidate_old_data(reader);

	const Http2ProcessFrameResult process_result =
	    process_http2_frame(context, frame, buffered_reader_get_connection_descriptor(reader));

	if(process_result.type != Http2ProcessFrameResultTypeOk) {
		const char* error =
		    process_result.type == Http2ProcessFrameResultTypeError
		        ? process_result.value.error
		        : "error: implementation error (settings frame can't result in a request)";
		return (Http2StartResult){ .is_error = true, .value = { .error = error } };
	}

	return (Http2StartResult){ .is_error = false };
}

static void free_http2_streams(Http2StreamMap streams) {
	// TODO
	UNUSED(streams);
}

static void free_http2_frames(Http2Frames frames) {
	for(size_t i = 0; i < TVEC_LENGTH(Http2Frame, frames); ++i) {
		const Http2Frame entry = TVEC_AT(Http2Frame, frames, i);

		free_http2_frame(entry);
	}

	TVEC_FREE(Http2Frame, &frames);
}

void free_http2_context(HTTP2Context context) {

	free_http2_streams(context.streams);
	free_http2_frames(context.frames);
}
