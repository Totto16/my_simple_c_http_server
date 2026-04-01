#include "./v2.h"
#include "./hpack.h"
#include "generic/send.h"
#include "generic/serialize.h"
#include "http/header.h"
#include "http/parser.h"

TVEC_IMPLEMENT_VEC_TYPE(Http2SettingSingleValue)

TMAP_IMPLEMENT_MAP_TYPE(Http2Identifier, StreamIdentifier, Http2Stream, Http2StreamMap)

TVEC_IMPLEMENT_VEC_TYPE(SizedBuffer)

TMAP_HASH_FUNC_SIG(Http2Identifier, StreamIdentifier) {
	const uint32_t value = key.identifier;
	return TMAP_HASH_SCALAR(value);
}

TMAP_EQ_FUNC_SIG(Http2Identifier, StreamIdentifier) {
	return (int32_t)(key1.identifier) == ((int32_t)key2.identifier);
}

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
NODISCARD static uint32_t special_impl_deserialize_u24_be_to_host(const uint8_t* bytes) {

	const uint8_t actual_value[4] = { 0, bytes[0], bytes[1], bytes[2] };

	return deserialize_u32_be_to_host(actual_value);
}

NODISCARD static Http2RawHeader parse_http2_raw_header(const uint8_t* const header_data) {

	uint8_t type = header_data[3];

	uint8_t flags = header_data[4];

	uint32_t length = special_impl_deserialize_u24_be_to_host(header_data);

	Http2Identifier stream_identifier =
	    deserialize_identifier(header_data + 5); // NOLINT(readability-magic-numbers)

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

NODISCARD inline static Http2Settings http2_default_settings(void) {
	return (Http2Settings){
		.header_table_size = DEFAULT_HEADER_TABLE_SIZE,
		.enable_push = true,
		.max_concurrent_streams = DEFAULT_MAX_CONCURRENT_STREAMS,
		.initial_window_size = DEFAULT_INITIAL_WINDOW_SIZE,
		.max_frame_size = DEFAULT_SETTINGS_MAX_FRAME_SIZE,
		.max_header_list_size = DEFAULT_MAX_HEADER_LIST_SIZE,
	};
}

NODISCARD static Http2HpackState get_default_hpack_state(size_t max_dynamic_table_byte_size) {
	return (Http2HpackState){
		.decompress_state = get_default_hpack_decompress_state(max_dynamic_table_byte_size),
		.compress_state = get_default_hpack_compress_state(max_dynamic_table_byte_size),
	};
}

NODISCARD inline static Http2ContextState http2_default_context_state(void) {
	return (Http2ContextState){
		.last_stream_id = { .identifier = 0 },
		.state = (Http2State){ .type = Http2StateTypeOpen },
		.hpack_state = get_default_hpack_state(DEFAULT_HEADER_TABLE_SIZE),
	};
}

NODISCARD HTTP2Context http2_default_context(void) {

	return (HTTP2Context){
		.settings = http2_default_settings(),
		.streams = TMAP_EMPTY(Http2StreamMap),
		.state = http2_default_context_state(),
	};
}

typedef struct {
	bool is_error;
	union {
		Http2Frame frame;
		tstr_static error;
	} data;
} Http2FrameResult;

NODISCARD static GenericResult http2_send_raw_frame(const ConnectionDescriptor* const descriptor,
                                                    const Http2RawHeader header,
                                                    const SizedBuffer data) {

	// TODO(Totto): support padding

	if(data.size != header.length) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	uint8_t header_buffer[HTTP2_HEADER_SIZE] = ZERO_ARRAY();

	{
		size_t i = 0;

		uint8_t* header_buffer_data = (uint8_t*)header_buffer;

		const SerializeResult32 length_res = serialize_u32_host_to_be(header.length);

		header_buffer_data[i++] = length_res.bytes[1];
		header_buffer_data[i++] = length_res.bytes[2];
		header_buffer_data[i++] = length_res.bytes[3];

		header_buffer_data[i++] = header.type;

		header_buffer_data[i++] = header.flags;

		const SerializeResult32 stream_id_res = serialize_identifier(header.stream_identifier);

		header_buffer_data[i++] = stream_id_res.bytes[0];
		header_buffer_data[i++] = stream_id_res.bytes[1];
		header_buffer_data[i++] = stream_id_res.bytes[2];
		header_buffer_data[i++] = stream_id_res.bytes[3];

		assert(i == HTTP2_HEADER_SIZE &&
		       "implemented http2 frame header serialization incorrectly");
	}

	GenericResult result = send_data_to_connection(descriptor, header_buffer, HTTP2_HEADER_SIZE);

	IF_GENERIC_RESULT_IS_ERROR_IGN(result) {
		return result;
	}

	result = send_buffer_to_connection(descriptor, data);

	return result;
}

#define HTTP2_FRAME_GOAWAY_BASE_SIZE ((32 + 32) / 8)

#define HTTP2_CONNECTION_STREAM_IDENTIFIER ((Http2Identifier){ .identifier = 0 })

NODISCARD static GenericResult http2_send_goaway_frame(const ConnectionDescriptor* const descriptor,
                                                       const Http2Identifier last_stream_id,
                                                       const Http2ErrorCode error_code,
                                                       const ReadonlyBuffer additional_debug_data) {

	uint32_t length = additional_debug_data.size + HTTP2_FRAME_GOAWAY_BASE_SIZE;

	Http2RawHeader header = {
		.length = length,
		.type = Http2FrameTypeGoaway,
		.flags = 0,
		.stream_identifier = HTTP2_CONNECTION_STREAM_IDENTIFIER,
	};

	SizedBuffer frame_as_data = allocate_sized_buffer(length);

	if(frame_as_data.data == NULL) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	{
		size_t i = 0;

		uint8_t* data = (uint8_t*)frame_as_data.data;

		const SerializeResult32 last_stream_id_res = serialize_identifier(last_stream_id);

		data[i++] = last_stream_id_res.bytes[0];
		data[i++] = last_stream_id_res.bytes[1];
		data[i++] = last_stream_id_res.bytes[2];
		data[i++] = last_stream_id_res.bytes[3];

		const SerializeResult32 error_code_res = serialize_u32_host_to_be(error_code);

		data[i++] = error_code_res.bytes[0];
		data[i++] = error_code_res.bytes[1];
		data[i++] = error_code_res.bytes[2];
		data[i++] = error_code_res.bytes[3];

		/* Optional debug data */
		if(additional_debug_data.size > 0) {
			memcpy((void*)(data + i), additional_debug_data.data, additional_debug_data.size);
			i += additional_debug_data.size;
		}

		assert(i == length && "implemented goaway serialization incorrectly");
	}

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame_as_data);

	free_sized_buffer(frame_as_data);

	return result;
}

#define HTTP2_RST_STREAM_SIZE (32 / 8)

NODISCARD static GenericResult
http2_send_rst_stream_frame(const ConnectionDescriptor* const descriptor,
                            const Http2RstStreamFrame frame) {

	Http2RawHeader header = {
		.length = HTTP2_RST_STREAM_SIZE,
		.type = Http2FrameTypeRstStream,
		.flags = 0,
		.stream_identifier = frame.identifier,
	};

	uint8_t frame_as_data_raw[HTTP2_RST_STREAM_SIZE] = ZERO_ARRAY();

	{
		size_t i = 0;

		uint8_t* data = (uint8_t*)frame_as_data_raw;

		const SerializeResult32 error_code_res = serialize_u32_host_to_be(frame.error_code);

		data[i++] = error_code_res.bytes[0];
		data[i++] = error_code_res.bytes[1];
		data[i++] = error_code_res.bytes[2];
		data[i++] = error_code_res.bytes[3];

		assert(i == HTTP2_RST_STREAM_SIZE && "implemented rst stream serialization incorrectly");
	}

	SizedBuffer frame_as_data = { .data = frame_as_data_raw, .size = HTTP2_RST_STREAM_SIZE };

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame_as_data);

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

NODISCARD static GenericResult
http2_send_settings_frame(const ConnectionDescriptor* const descriptor, Http2SettingsFrame frame) {

	uint32_t length = TVEC_LENGTH(Http2SettingSingleValue, frame.entries);

	uint8_t flags =
	    frame.ack ? Http2SettingsFrameFlagAck : 0; // NOLINT(readability-implicit-bool-conversion)

	Http2RawHeader header = {
		.length = length,
		.type = Http2FrameTypeSettings,
		.flags = flags,
		.stream_identifier = HTTP2_CONNECTION_STREAM_IDENTIFIER,
	};

	if(frame.ack && length != 0) { // NOLINT(readability-implicit-bool-conversion)
		return GENERIC_RES_ERR_UNIQUE();
	}

	SizedBuffer frame_as_data = allocate_sized_buffer(length);

	if(frame_as_data.data == NULL) {
		return GENERIC_RES_ERR_UNIQUE();
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
			data[i + 5] = // NOLINT(readability-magic-numbers)
			    value_res.bytes[3];
		}
	}

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame_as_data);

	free_sized_buffer(frame_as_data);

	return result;
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

NODISCARD static GenericResult http2_send_ping_frame(const ConnectionDescriptor* const descriptor,
                                                     Http2PingFrame frame) {

	uint8_t flags =
	    frame.ack ? Http2PingFrameFlagAck : 0; // NOLINT(readability-implicit-bool-conversion)

	Http2RawHeader header = {
		.length = HTTP2_PING_FRAME_SIZE,
		.type = Http2FrameTypePing,
		.flags = flags,
		.stream_identifier = HTTP2_CONNECTION_STREAM_IDENTIFIER,
	};

	if(frame.opaque_data.size != HTTP2_PING_FRAME_SIZE) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	uint8_t frame_as_data_raw[HTTP2_PING_FRAME_SIZE] = ZERO_ARRAY();

	{

		memcpy(frame_as_data_raw, frame.opaque_data.data, HTTP2_PING_FRAME_SIZE);
	}

	SizedBuffer frame_as_data = { .data = frame_as_data_raw, .size = HTTP2_PING_FRAME_SIZE };

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame_as_data);

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

NODISCARD static GenericResult http2_send_data_frame(const ConnectionDescriptor* const descriptor,
                                                     const Http2DataFrame frame) {

	Http2RawHeader header = {
		.length = frame.content.size,
		.type = Http2FrameTypeData,
		.flags = frame.is_end // NOLINT(readability-implicit-bool-conversion)
		             ? Http2DataFrameFlagEndStream
		             : 0,
		.stream_identifier = frame.identifier,
	};

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame.content);

	return result;
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

NODISCARD static GenericResult
http2_send_headers_frame(const ConnectionDescriptor* const descriptor,
                         const Http2HeadersFrame frame) {

	// TODO(Totto): support priority_opt
	assert(frame.priority_opt.has_priority == false && "not yet implemented");

	Http2HeadersFrameFlag flags =
	    (Http2HeadersFrameFlag)0; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

	if(frame.end_stream) {
		flags =                             // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
		    flags |                         // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
		    Http2HeadersFrameFlagEndStream; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	}

	if(frame.end_headers) {
		flags =                              // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
		    flags |                          // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
		    Http2HeadersFrameFlagEndHeaders; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	}

	Http2RawHeader header = {
		.length = frame.block_fragment.size,
		.type = Http2FrameTypeHeaders,
		.flags = flags,
		.stream_identifier = frame.identifier,
	};

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame.block_fragment);

	return result;
}

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2ContinuationFrameFlagEndHeaders = Http2FrameFlagEndHeaders,
	// all allowed flags or-ed together
	Http2ContinuationFrameFlagsAllowed = Http2ContinuationFrameFlagEndHeaders
} Http2ContinuationFrameFlag;

NODISCARD static GenericResult
http2_send_continuation_frame(const ConnectionDescriptor* const descriptor,
                              const Http2ContinuationFrame frame) {
	Http2RawHeader header = {
		.length = frame.block_fragment.size,
		.type = Http2FrameTypeContinuation,
		.flags = frame.end_headers ? // NOLINT(readability-implicit-bool-conversion)
		             Http2ContinuationFrameFlagEndHeaders
		                           : 0,
		.stream_identifier = frame.identifier,
	};

	const GenericResult result = http2_send_raw_frame(descriptor, header, frame.block_fragment);

	return result;
}

NODISCARD GenericResult http2_send_connection_error(const ConnectionDescriptor* const descriptor,
                                                    const HTTP2Context* const context,
                                                    Http2ErrorCode error_code,
                                                    const tstr_static error) {

	if(tstr_static_is_null(error)) {
		return http2_send_connection_error_with_data(descriptor, context, error_code,
		                                             (ReadonlyBuffer){ .data = NULL, .size = 0 });
	};

	return http2_send_connection_error_with_data(
	    descriptor, context, error_code, (ReadonlyBuffer){ .data = error.ptr, .size = error.len });
}

NODISCARD GenericResult http2_send_connection_error_with_data(
    const ConnectionDescriptor* const descriptor, const HTTP2Context* const context,
    Http2ErrorCode error_code, const ReadonlyBuffer debug_data) {

	return http2_send_goaway_frame(descriptor, context->state.last_stream_id, error_code,
	                               debug_data);
}

NODISCARD GenericResult http2_send_stream_error(const ConnectionDescriptor* descriptor,
                                                Http2ErrorCode error_code,
                                                Http2Identifier stream_identifier) {

	Http2RstStreamFrame frame = {
		.error_code = error_code,
		.identifier = stream_identifier,
	};

	return http2_send_rst_stream_frame(descriptor, frame);
}

NODISCARD static Http2FrameResult parse_http2_data_frame(BufferedReader* const reader,
                                                         const HTTP2Context* const context,
                                                         Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const tstr_static error = TSTR_STATIC_LIT("Data Frame doesn't allow stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2DataFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid data frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	uint8_t padding_length = 0;
	size_t payload_length = http2_raw_header.length;

	if((http2_raw_header.flags & Http2DataFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const tstr_static error =
			    TSTR_STATIC_LIT("not enough frame data for padding length field(1 byte)");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.buffer;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const tstr_static error =
			    TSTR_STATIC_LIT("padding length is greater than remaining length in frame");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error = TSTR_STATIC_LIT("Failed to read enough data for the frame data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = read_result.value.buffer;

	if(frame_data.size == 0) {
		frame_data.data = NULL;
	} else {
		frame_data = sized_buffer_dup(frame_data);

		if(frame_data.data == NULL) {
			const tstr_static error = TSTR_STATIC_LIT("Failed allocate frame data content buffer");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	if(padding_length != 0) {
		read_result = buffered_reader_get_amount(reader, padding_length);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the padding data");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result.value.buffer;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const tstr_static error = TSTR_STATIC_LIT("padding bytes are not 0");
				const GenericResult _ =
				    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
				                                context, Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
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

#define HTTP2_PRIORITY_INFO_SIZE ((32 + 8) / 8)

NODISCARD static Http2FramePriority
get_http2_priority_info_from_raw_data(const SizedBuffer raw_data) {
	uint8_t* priority_data_raw = (uint8_t*)raw_data.data;

	uint32_t stream_dependency_identifier_raw = deserialize_u32_be_to_host(priority_data_raw);

	uint32_t dependency_identifier =
	    stream_dependency_identifier_raw & 0x7FFFFFFF; // NOLINT(readability-magic-numbers)

	bool exclusive =
	    (stream_dependency_identifier_raw >> 31) != // NOLINT(readability-magic-numbers)
	    0;

	uint8_t weight = priority_data_raw[4];

	Http2FramePriority priority = {
		.exclusive = exclusive,
		.dependency_identifier = dependency_identifier,
		.weight = weight,
	};

	return priority;
}

// See: https://datatracker.ietf.org/doc/html/rfc7540#section-5.3.5
#define DEFAULT_STREAM_PRIORITY_WEIGHT 16

#define DEFAULT_STREAM_PRIORITY_EXT(identifier) \
	((Http2FramePriority){ .exclusive = false, \
	                       .dependency_identifier = (identifier), \
	                       .weight = DEFAULT_STREAM_PRIORITY_WEIGHT })

#define DEFAULT_STREAM_PRIORITY DEFAULT_STREAM_PRIORITY_EXT(0x00)

NODISCARD static Http2FrameResult parse_http2_headers_frame(BufferedReader* const reader,
                                                            const HTTP2Context* const context,
                                                            Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const tstr_static error = TSTR_STATIC_LIT("Headers Frame doesn't allow stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2HeadersFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid headers frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	size_t payload_length = http2_raw_header.length;

	uint8_t padding_length = 0;

	if((http2_raw_header.flags & Http2DataFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const tstr_static error =
			    TSTR_STATIC_LIT("not enough frame data for padding length field(1 byte)");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.buffer;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const tstr_static error =
			    TSTR_STATIC_LIT("padding length is greater than remaining length in frame");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	Http2FramePriorityOptional priority_opt = {
		.has_priority = (http2_raw_header.flags & Http2HeadersFrameFlagPriority) != 0,
		.priority = DEFAULT_STREAM_PRIORITY
	};

	if(priority_opt.has_priority) {
		if(payload_length < HTTP2_PRIORITY_INFO_SIZE) {
			const tstr_static error = TSTR_STATIC_LIT("not enough frame data for priority info");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - HTTP2_PRIORITY_INFO_SIZE;

		BufferedReadResult read_result =
		    buffered_reader_get_amount(reader, HTTP2_PRIORITY_INFO_SIZE);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer priority_data = read_result.value.buffer;

		Http2FramePriority priority = get_http2_priority_info_from_raw_data(priority_data);

		priority_opt.priority = priority;
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error = TSTR_STATIC_LIT("Failed to read enough data for the frame data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer block_fragment = read_result.value.buffer;

	if(block_fragment.size == 0) {
		block_fragment.data = NULL;
	} else {
		block_fragment = sized_buffer_dup(block_fragment);

		if(block_fragment.data == NULL) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed allocate headers block fragment buffer");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	if(padding_length != 0) {
		read_result = buffered_reader_get_amount(reader, padding_length);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the padding data");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result.value.buffer;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const tstr_static error = TSTR_STATIC_LIT("padding bytes are not 0");
				const GenericResult _ =
				    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
				                                context, Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
		}
	}

	Http2HeadersFrame headers_frame = {
		.priority_opt = priority_opt,
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
                                                             const HTTP2Context* const context,
                                                             Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const tstr_static error = TSTR_STATIC_LIT("Priority Frame doesn't allow stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2PriorityFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid priority frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	if(payload_length != HTTP2_PRIORITY_INFO_SIZE) {
		const tstr_static error = TSTR_STATIC_LIT("not enough frame data for priority info");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_PRIORITY_INFO_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error =
		    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer priority_data = read_result.value.buffer;

	Http2FramePriority priority = get_http2_priority_info_from_raw_data(priority_data);

	Http2PriorityFrame priority_frame = {
		.priority = priority,
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

NODISCARD static Http2FrameResult parse_http2_rst_stream_frame(BufferedReader* const reader,
                                                               const HTTP2Context* const context,
                                                               Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const tstr_static error = TSTR_STATIC_LIT("Rst stream Frame doesn't allow stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2RestStreamFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid rst stream frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	if(payload_length != HTTP2_RST_STREAM_SIZE) {
		const tstr_static error = TSTR_STATIC_LIT("not enough frame data for rst stream data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_RST_STREAM_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error =
		    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer error_code_data = read_result.value.buffer;

	const uint32_t error_code = deserialize_u32_be_to_host((uint8_t*)error_code_data.data);

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

#define HTTP2_SETTINGS_FRAME_SINGLE_SIZE 6

#define FREE_AT_END() \
	do { \
		TVEC_FREE(Http2SettingSingleValue, &settings_frame.entries); \
	} while(false)

NODISCARD static Http2FrameResult parse_raw_http2_settings_frame(const SizedBuffer input,
                                                                 BufferedReader* const reader,
                                                                 const HTTP2Context* const context,
                                                                 const bool ack) {

	Http2SettingsFrame settings_frame = {
		.ack = ack,
		.entries = TVEC_EMPTY(Http2SettingSingleValue),
	};

	if((input.size % HTTP2_SETTINGS_FRAME_SINGLE_SIZE) != 0) {
		const tstr_static error =
		    TSTR_STATIC_LIT("invalid settings frame length, not a multiple of 6");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);

		FREE_AT_END();
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(input.size > 0) {

		uint8_t* data = (uint8_t*)input.data;

		for(size_t i = 0; i < input.size; i += HTTP2_SETTINGS_FRAME_SINGLE_SIZE) {
			uint16_t identifier = deserialize_u16_be_to_host(data + i);

			uint32_t value = deserialize_u32_be_to_host(data + i + 2);

			switch(identifier) {
				case Http2SettingsFrameIdentifierHeaderTableSize: {
					break;
				}
				case Http2SettingsFrameIdentifierEnablePush: {
					if(value != 0 && value != 1) {
						const tstr_static error =
						    TSTR_STATIC_LIT("Invalid SETTINGS_ENABLE_PUSH settings value");
						const GenericResult _ = http2_send_connection_error(
						    buffered_reader_get_connection_descriptor(reader), context,
						    Http2ErrorCodeProtocolError, error);
						UNUSED(_);

						FREE_AT_END();
						return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
					}
					break;
				}
				case Http2SettingsFrameIdentifierMaxConcurrentStreams: {
					break;
				}
				case Http2SettingsFrameIdentifierInitialWindowSize: {

					if(value > MAX_FLOW_CONTROL_WINDOW_SIZE) {
						const tstr_static error =
						    TSTR_STATIC_LIT("Invalid SETTINGS_INITIAL_WINDOW_SIZE settings value");
						const GenericResult _ = http2_send_connection_error(
						    buffered_reader_get_connection_descriptor(reader), context,
						    Http2ErrorCodeFlowControlError, error);
						UNUSED(_);

						FREE_AT_END();
						return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
					}
					break;
				}
				case Http2SettingsFrameIdentifierMaxFrameSize: {
					if(value > MAX_MAX_FRAME_SIZE + 1 || value < DEFAULT_SETTINGS_MAX_FRAME_SIZE) {
						const tstr_static error =
						    TSTR_STATIC_LIT("Invalid SETTINGS_MAX_FRAME_SIZE settings value");
						const GenericResult _ = http2_send_connection_error(
						    buffered_reader_get_connection_descriptor(reader), context,
						    Http2ErrorCodeProtocolError, error);
						UNUSED(_);

						FREE_AT_END();
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

			const TvecResult push_res =
			    TVEC_PUSH(Http2SettingSingleValue, &settings_frame.entries, entry);
			if(push_res != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)

				FREE_AT_END();
				return (Http2FrameResult){ .is_error = true,
					                       .data = { .error = TSTR_STATIC_LIT(
					                                     "OOM in settings array push") } };
			}
		}
	}

	Http2Frame frame = { .type = Http2FrameTypeSettings,
		                 .value = {
		                     .settings = settings_frame,
		                 } };

	return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
}

#undef FREE_AT_END

NODISCARD static Http2FrameResult parse_http2_settings_frame(BufferedReader* const reader,
                                                             const HTTP2Context* const context,
                                                             Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const tstr_static error = TSTR_STATIC_LIT("Settings Frame only allows stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2SettingsFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid settings frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const bool ack = (http2_raw_header.flags & Http2SettingsFrameFlagAck) != 0;

	if(ack) {
		if(http2_raw_header.length != 0) {
			const tstr_static error =
			    TSTR_STATIC_LIT("ack in settings frame with a non zero payload is invalid");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeFrameSizeError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		Http2Frame frame = { .type = Http2FrameTypeSettings,
			                 .value = {
			                     .settings =
			                         (Http2SettingsFrame){
			                             .ack = ack,
			                             .entries = TVEC_EMPTY(Http2SettingSingleValue),
			                         },
			                 } };
		return (Http2FrameResult){ .is_error = false, .data = { .frame = frame } };
	}

	if((http2_raw_header.length % HTTP2_SETTINGS_FRAME_SINGLE_SIZE) != 0) {
		const tstr_static error =
		    TSTR_STATIC_LIT("invalid settings frame length, not a multiple of 6");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = (SizedBuffer){ .data = NULL, .size = 0 };

	if(http2_raw_header.length > 0) {

		BufferedReadResult read_result =
		    buffered_reader_get_amount(reader, http2_raw_header.length);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the frame data");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		frame_data = read_result.value.buffer;
	}

	return parse_raw_http2_settings_frame(frame_data, reader, context, ack);
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

NODISCARD static Http2FrameResult parse_http2_push_promise_frame(const HTTP2Context* const context,
                                                                 BufferedReader* const reader,
                                                                 Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const tstr_static error = TSTR_STATIC_LIT("Push promise Frame only allows stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2PushPromiseFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid push promise frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(!context->settings.enable_push) {
		const tstr_static error = TSTR_STATIC_LIT("push promise frames are disabled per settings");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	size_t payload_length = http2_raw_header.length;

	uint8_t padding_length = 0;

	if((http2_raw_header.flags & Http2PushPromiseFrameFlagPadded) != 0) {

		if(payload_length < 1) {
			const tstr_static error =
			    TSTR_STATIC_LIT("not enough frame data for padding length field(1 byte)");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		SizedBuffer padding_data = read_result.value.buffer;

		padding_length = ((uint8_t*)padding_data.data)[0];

		if(padding_length >= payload_length - 1) {
			const tstr_static error =
			    TSTR_STATIC_LIT("padding length is greater than remaining length in frame");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeProtocolError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		payload_length = payload_length - 1 - padding_length;
	}

	if(payload_length < 4) {
		const tstr_static error =
		    TSTR_STATIC_LIT("not enough frame data for promised stream identifier field(4 bytes)");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	payload_length = payload_length - 4;

	BufferedReadResult read_result = buffered_reader_get_amount(reader, 1);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error =
		    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer promised_stream_identifier_data = read_result.value.buffer;

	const Http2Identifier promised_stream_identifier =
	    deserialize_identifier((uint8_t*)promised_stream_identifier_data.data);

	read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error = TSTR_STATIC_LIT("Failed to read enough data for the frame data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer block_fragment = read_result.value.buffer;

	if(block_fragment.size == 0) {
		block_fragment.data = NULL;
	} else {
		block_fragment = sized_buffer_dup(block_fragment);

		if(block_fragment.data == NULL) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed allocate headers block fragment buffer");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}

	if(padding_length != 0) {
		read_result = buffered_reader_get_amount(reader, padding_length);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the padding data");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
		SizedBuffer padding_data = read_result.value.buffer;

		for(size_t i = 0; i < padding_data.size; ++i) {
			uint8_t data = ((uint8_t*)padding_data.data)[i];

			if(data != 0) {
				const tstr_static error = TSTR_STATIC_LIT("padding bytes are not 0");
				const GenericResult _ =
				    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
				                                context, Http2ErrorCodeProtocolError, error);
				UNUSED(_);
				return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
			}
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

NODISCARD static Http2FrameResult parse_http2_ping_frame(BufferedReader* const reader,
                                                         const HTTP2Context* const context,
                                                         Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const tstr_static error = TSTR_STATIC_LIT("The ping Frame only allows stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2PingFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid ping frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	if(payload_length != HTTP2_PING_FRAME_SIZE) {
		const tstr_static error = TSTR_STATIC_LIT("not enough frame data for ping data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result = buffered_reader_get_amount(reader, HTTP2_PING_FRAME_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error =
		    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer opaque_data = sized_buffer_dup(read_result.value.buffer);

	if(opaque_data.data == NULL) {
		const tstr_static error = TSTR_STATIC_LIT("Failed allocate frame data content buffer");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
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
                                                           const HTTP2Context* const context,
                                                           Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier != 0) {
		const tstr_static error = TSTR_STATIC_LIT("The goaway Frame only allows stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2GoawayFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid goaway frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(http2_raw_header.length < BASE_HTTP2_GOAWAY_FRAME_SIZE) {
		const tstr_static error = TSTR_STATIC_LIT("invalid goaway frame length, not enough data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t additional_data_size = http2_raw_header.length - BASE_HTTP2_GOAWAY_FRAME_SIZE;

	BufferedReadResult read_result =
	    buffered_reader_get_amount(reader, BASE_HTTP2_GOAWAY_FRAME_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error = TSTR_STATIC_LIT("Failed to read enough data for the frame data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = read_result.value.buffer;

	uint8_t* data = (uint8_t*)frame_data.data;

	const Http2Identifier last_stream_id = deserialize_identifier(data);

	uint32_t error_code = deserialize_u32_be_to_host(data + sizeof(last_stream_id));

	SizedBuffer additional_debug_data = { .data = NULL, .size = 0 };

	if(additional_data_size > 0) {

		read_result = buffered_reader_get_amount(reader, additional_data_size);

		if(read_result.type != BufferedReadResultTypeOk) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed to read enough data for the frame data");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}

		additional_debug_data = sized_buffer_dup(read_result.value.buffer);

		if(additional_debug_data.data == NULL) {
			const tstr_static error = TSTR_STATIC_LIT("Failed allocate frame data content buffer");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
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
                                                                  const HTTP2Context* const context,
                                                                  Http2RawHeader http2_raw_header) {

	if((http2_raw_header.flags & Http2WindowUpdateFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid windows update frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if(http2_raw_header.length != HTTP2_WINDOW_UPDATE_FRAME_SIZE) {
		const tstr_static error =
		    TSTR_STATIC_LIT("invalid window update frame length, not enough data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	BufferedReadResult read_result =
	    buffered_reader_get_amount(reader, HTTP2_WINDOW_UPDATE_FRAME_SIZE);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error =
		    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer frame_data = read_result.value.buffer;

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

NODISCARD static Http2FrameResult parse_http2_continuation_frame(BufferedReader* const reader,
                                                                 const HTTP2Context* const context,
                                                                 Http2RawHeader http2_raw_header) {

	if(http2_raw_header.stream_identifier.identifier == 0) {
		const tstr_static error = TSTR_STATIC_LIT("Continuation Frame doesn't allow stream id 0");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	if((http2_raw_header.flags & Http2ContinuationFrameFlagsAllowed) != http2_raw_header.flags) {
		const tstr_static error = TSTR_STATIC_LIT("invalid continuation frame flags");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeProtocolError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	const size_t payload_length = http2_raw_header.length;

	BufferedReadResult read_result = buffered_reader_get_amount(reader, payload_length);

	if(read_result.type != BufferedReadResultTypeOk) {
		const tstr_static error = TSTR_STATIC_LIT("Failed to read enough data for the frame data");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer block_fragment = read_result.value.buffer;

	if(block_fragment.size == 0) {
		block_fragment.data = NULL;
	} else {
		block_fragment = sized_buffer_dup(block_fragment);

		if(block_fragment.data == NULL) {
			const tstr_static error =
			    TSTR_STATIC_LIT("Failed allocate headers block fragment buffer");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
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
		const tstr_static error =
		    TSTR_STATIC_LIT("Failed to read enough data for the frame header");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeInternalError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	SizedBuffer header_data = read_result.value.buffer;

	const Http2RawHeader http2_raw_header = parse_http2_raw_header(header_data.data);

	if(http2_raw_header.length > context->settings.max_frame_size) {
		const tstr_static error = TSTR_STATIC_LIT("Header size too big");
		const GenericResult _ =
		    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader), context,
		                                Http2ErrorCodeFrameSizeError, error);
		UNUSED(_);
		return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
	}

	switch(http2_raw_header.type) {
		case Http2FrameTypeData: {
			return parse_http2_data_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypeHeaders: {
			return parse_http2_headers_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypePriority: {
			return parse_http2_priority_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypeRstStream: {
			return parse_http2_rst_stream_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypeSettings: {
			return parse_http2_settings_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypePushPromise: {
			return parse_http2_push_promise_frame(context, reader, http2_raw_header);
		}
		case Http2FrameTypePing: {
			return parse_http2_ping_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypeGoaway: {
			return parse_http2_goaway_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypeWindowUpdate: {
			return parse_http2_window_update_frame(reader, context, http2_raw_header);
		}
		case Http2FrameTypeContinuation: {
			return parse_http2_continuation_frame(reader, context, http2_raw_header);
		}
		default: {
			const tstr_static error = TSTR_STATIC_LIT("Unrecognized frame type");
			const GenericResult _ =
			    http2_send_connection_error(buffered_reader_get_connection_descriptor(reader),
			                                context, Http2ErrorCodeInternalError, error);
			UNUSED(_);
			return (Http2FrameResult){ .is_error = true, .data = { .error = error } };
		}
	}
}

static void http2_apply_settings_frame(HTTP2Context* const context,
                                       const Http2SettingsFrame frame) {

	for(size_t i = 0; i < TVEC_LENGTH(Http2SettingSingleValue, frame.entries); ++i) {
		const Http2SettingSingleValue entry = TVEC_AT(Http2SettingSingleValue, frame.entries, i);

		switch(entry.identifier) {
			case Http2SettingsFrameIdentifierHeaderTableSize: {
				context->settings.header_table_size = entry.value;
				set_hpack_decompress_state_setting(context->state.hpack_state.decompress_state,
				                                   entry.value);
				break;
			}
			case Http2SettingsFrameIdentifierEnablePush: {
				context->settings.enable_push = entry.value != 0;
				break;
			}
			case Http2SettingsFrameIdentifierMaxConcurrentStreams: {
				context->settings.max_concurrent_streams = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierInitialWindowSize: {
				context->settings.initial_window_size = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierMaxFrameSize: {
				context->settings.max_frame_size = entry.value;
				break;
			}
			case Http2SettingsFrameIdentifierMaxHeaderListSize: {
				context->settings.max_header_list_size = entry.value;
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

static void free_http2_frame(const Http2Frame* const frame) {

	switch(frame->type) {
		case Http2FrameTypeData: {
			free_http2_data_frame(frame->value.data);
			break;
		}
		case Http2FrameTypeHeaders: {
			free_http2_headers_frame(frame->value.headers);
			break;
		}
		case Http2FrameTypePriority: {
			free_http2_priority_frame(frame->value.priority);
			break;
		}
		case Http2FrameTypeRstStream: {
			free_http2_rst_stream_frame(frame->value.rst_stream);
			break;
		}
		case Http2FrameTypeSettings: {
			free_http2_settings_frame(frame->value.settings);
			break;
		}
		case Http2FrameTypePushPromise: {
			free_http2_push_promise_frame(frame->value.push_promise);
			break;
		}
		case Http2FrameTypePing: {
			free_http2_ping_frame(frame->value.ping);
			break;
		}
		case Http2FrameTypeGoaway: {
			free_http2_goaway_frame(frame->value.goaway);
			break;
		}
		case Http2FrameTypeWindowUpdate: {
			free_http2_window_update_frame(frame->value.window_update);
			break;
		}
		case Http2FrameTypeContinuation: {
			free_http2_continuation_frame(frame->value.continuation);
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
	Http2ProcessFrameResultTypeError,
	Http2ProcessFrameResultTypeCloseConnection
} Http2ProcessFrameResultType;

typedef struct {
	Http2Identifier identifier;
} Http2ProcessFrameFinishedRequest;

typedef struct {
	bool is_connection_error;
	tstr_static message;
} Http2ProcessFrameError;

typedef struct {
	Http2ProcessFrameResultType type;
	union {
		Http2ProcessFrameError error;
		Http2ProcessFrameFinishedRequest request;
	} value;
} Http2ProcessFrameResult;

typedef struct {
	bool is_associated_with_stream;
	union {
		Http2Identifier stream_identifier;
	} value;
} Http2FrameCategory;

NODISCARD static Http2FrameCategory get_http2_frame_category(const Http2Frame frame) {
	switch(frame.type) {
		case Http2FrameTypeData: {
			const Http2DataFrame data_frame = frame.value.data;

			return (Http2FrameCategory){ .is_associated_with_stream = true,
				                         .value = { .stream_identifier = data_frame.identifier } };
		}
		case Http2FrameTypeHeaders: {
			const Http2HeadersFrame headers_frame = frame.value.headers;

			return (
			    Http2FrameCategory){ .is_associated_with_stream = true,
				                     .value = { .stream_identifier = headers_frame.identifier } };
		}
		case Http2FrameTypePriority: {
			const Http2PriorityFrame priority_frame = frame.value.priority;

			return (
			    Http2FrameCategory){ .is_associated_with_stream = true,
				                     .value = { .stream_identifier = priority_frame.identifier } };
		}
		case Http2FrameTypeRstStream: {
			const Http2RstStreamFrame rst_stream_frame = frame.value.rst_stream;

			return (Http2FrameCategory){ .is_associated_with_stream = true,
				                         .value = { .stream_identifier =
				                                        rst_stream_frame.identifier } };
		}
		case Http2FrameTypeSettings: {
			return (Http2FrameCategory){ .is_associated_with_stream = false };
		}
		case Http2FrameTypePushPromise: {
			const Http2PushPromiseFrame push_promise_frame = frame.value.push_promise;

			return (Http2FrameCategory){ .is_associated_with_stream = true,
				                         .value = { .stream_identifier =
				                                        push_promise_frame.identifier } };
		}
		case Http2FrameTypePing:
		case Http2FrameTypeGoaway: {
			return (Http2FrameCategory){ .is_associated_with_stream = false };
		}
		case Http2FrameTypeWindowUpdate: {
			const Http2WindowUpdateFrame window_update_frame = frame.value.window_update;

			if(window_update_frame.identifier.identifier == 0) {
				return (Http2FrameCategory){ .is_associated_with_stream = false };
			}

			return (Http2FrameCategory){ .is_associated_with_stream = true,
				                         .value = { .stream_identifier =
				                                        window_update_frame.identifier } };
		}
		case Http2FrameTypeContinuation: {
			const Http2ContinuationFrame continuation_frame = frame.value.continuation;

			return (Http2FrameCategory){ .is_associated_with_stream = true,
				                         .value = { .stream_identifier =
				                                        continuation_frame.identifier } };
		}
		default: {
			UNREACHABLE(); // NOLINT(cert-dcl03-c,misc-static-assert)
		}
	}
}
NODISCARD static Http2Stream* get_http2_stream(HTTP2Context* const context,
                                               const Http2Identifier stream_identifier) {

	Http2Stream* stream = TMAP_GET_MUT(Http2StreamMap, &(context->streams), stream_identifier);

	return stream;
}

NODISCARD static Http2StreamState get_http2_stream_state(const HTTP2Context* const context,
                                                         const Http2Identifier stream_identifier) {

	const Http2Stream* stream = TMAP_GET(Http2StreamMap, &(context->streams), stream_identifier);

	if(stream == NULL) {
		return Http2StreamStateIdle;
	}

	return stream->state;
}

NODISCARD static bool add_new_http2_stream(HTTP2Context* const context,
                                           const Http2Identifier stream_identifier,
                                           const Http2Stream stream) {

	const TmapInsertResult result =
	    TMAP_INSERT(Http2StreamMap, &(context->streams), stream_identifier, stream, false);

	return result == TmapInsertResultOk;
}

#define EMPTY_STREAM_HEADERS \
	((Http2StreamHeaders){ .finished = false, .header_blocks = TVEC_EMPTY(SizedBuffer) })
#define EMPTY_STREAM_CONTENT \
	((Http2StreamContent){ .finished = false, .data_blocks = TVEC_EMPTY(SizedBuffer) })

NODISCARD static bool http2_stream_add_header_block(Http2Stream* const stream,
                                                    SizedBuffer* const header_block,
                                                    bool finished) {

	if(stream->headers.finished) {
		return false;
	}

	TvecResult result = TVEC_PUSH(SizedBuffer, &(stream->headers.header_blocks), *header_block);
	if(result != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)
		return false;
	}

	*header_block = (SizedBuffer){ .data = NULL, .size = 0 };
	stream->headers.finished = finished;

	return true;
}

NODISCARD static bool http2_stream_add_content_block(Http2Stream* const stream,
                                                     SizedBuffer* const content_block,
                                                     bool end_stream) {

	if(stream->end_stream) {
		return false;
	}

	TvecResult result = TVEC_PUSH(SizedBuffer, &(stream->content.data_blocks), *content_block);
	if(result != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)
		return false;
	}

	*content_block = (SizedBuffer){ .data = NULL, .size = 0 };
	stream->end_stream = end_stream;

	return true;
}

static void free_http2_stream_headers(Http2StreamHeaders* const headers) {

	for(size_t i = 0; i < TVEC_LENGTH(SizedBuffer, headers->header_blocks); ++i) {
		SizedBuffer entry = TVEC_AT(SizedBuffer, headers->header_blocks, i);

		free_sized_buffer(entry);
	}

	TVEC_FREE(SizedBuffer, &(headers->header_blocks));
}

static void free_http2_stream_content(Http2StreamContent* const content) {

	for(size_t i = 0; i < TVEC_LENGTH(SizedBuffer, content->data_blocks); ++i) {
		SizedBuffer entry = TVEC_AT(SizedBuffer, content->data_blocks, i);

		free_sized_buffer(entry);
	}

	TVEC_FREE(SizedBuffer, &(content->data_blocks));
}

static void free_http2_stream(Http2Stream* const stream) {
	free_http2_stream_headers(&(stream->headers));
	free_http2_stream_content(&(stream->content));
}

static void http2_close_stream(Http2Stream* const stream) {

	if(stream == NULL) {
		return;
	}

	stream->state = Http2StreamStateClosed;
	free_http2_stream(stream);
	// TODO(Totto): when to remove this stream from the map?
}

static void http2_close_stream_by_identifier(HTTP2Context* const context,
                                             const Http2Identifier identifier) {
	Http2Stream* stream = get_http2_stream(context, identifier);

	if(stream == NULL) {
		return;
	}

	http2_close_stream(stream);
}

NODISCARD static Http2ProcessFrameResult
process_http2_frame_for_stream(const Http2Identifier stream_identifier, HTTP2Context* const context,
                               Http2Frame* frame, ConnectionDescriptor* const descriptor) {

	// this handles stream states, see:
	// https://datatracker.ietf.org/doc/html/rfc7540#section-5.1

	const Http2StreamState stream_state = get_http2_stream_state(context, stream_identifier);

	switch(frame->type) {
		case Http2FrameTypeData: {
			Http2DataFrame* data_frame = &(frame->value.data);

			if(stream_state != Http2StreamStateOpen && stream_state != Http2StreamStateHalfClosed) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Data frame send on a stream in an invalid state");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeStreamClosed, stream_identifier);
				UNUSED(_);

				http2_close_stream_by_identifier(context, stream_identifier);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = {
						.is_connection_error = false,
						.message = error,
					} },
				};
			}

			Http2Stream* stream = get_http2_stream(context, stream_identifier);

			if(stream == NULL) {
				const tstr_static error = TSTR_STATIC_LIT("Implementation error, stream not found");
				const GenericResult _ = http2_send_connection_error(
				    descriptor, context, Http2ErrorCodeInternalError, error);
				UNUSED(_);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = true, .message = error } },
				};
			}

			if(stream->end_stream) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Stream already finished but still received a data frame");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream(stream);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			if(!stream->headers.finished) {
				const tstr_static error = TSTR_STATIC_LIT(
				    "Stream headers not finished, but already received a data frame");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream(stream);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			// this pushes the block, sets it in the frame to NULL, so that the frame freeing
			// doesn't clear the buffer!
			if(!http2_stream_add_content_block(stream, &(data_frame->content),
			                                   data_frame->is_end)) {
				return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
					                              .value = {
					                                  .error = { .is_connection_error = true,
					                                             .message = TSTR_STATIC_LIT(
					                                                 "stream content block add "
					                                                 "error") },
					                              } };
			}

			if(stream->end_stream) {

				// TODO(Totto): maybe set some special flag or state in the stream?
				stream->state = Http2StreamStateHalfClosed;

				return (Http2ProcessFrameResult){ .type =
					                                  Http2ProcessFrameResultTypeNewFinishedRequest,
					                              .value = {
					                                  .request =
					                                      (Http2ProcessFrameFinishedRequest){
					                                          .identifier = stream_identifier },
					                              } };
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeHeaders: {
			Http2HeadersFrame* headers_frame = &(frame->value.headers);

			if(stream_state == Http2StreamStateClosed) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Headers frame send on a stream in an invalid state");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, headers_frame->identifier);
				UNUSED(_);

				http2_close_stream_by_identifier(context, stream_identifier);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			// TODO(Totto): support trailers, only those can be send on non idle state
			if(stream_state != Http2StreamStateIdle) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Headers frame send on a stream in an invalid state (trailers "
				                    "not supported yet)");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, headers_frame->identifier);
				UNUSED(_);

				http2_close_stream_by_identifier(context, stream_identifier);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			Http2Stream* stream = get_http2_stream(context, stream_identifier);

			if(stream == NULL) {

				Http2Stream new_stream = {
					.state = Http2StreamStateOpen,
					.headers = EMPTY_STREAM_HEADERS,
					.content = EMPTY_STREAM_CONTENT,
					.end_stream = headers_frame->end_stream,
					.priority = DEFAULT_STREAM_PRIORITY,
				};

				if(headers_frame->priority_opt.has_priority) {
					new_stream.priority = headers_frame->priority_opt.priority;
				}

				if(!add_new_http2_stream(context, stream_identifier, new_stream)) {
					return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
						                              .value = {
						                                  .error = { .is_connection_error = true,
						                                             .message = TSTR_STATIC_LIT(
						                                                 "stream insert error") },
						                              } };
				}

				stream = get_http2_stream(context, stream_identifier);

				if(stream == NULL) {
					const tstr_static error =
					    TSTR_STATIC_LIT("Implementation error, stream not found (after insert)");
					const GenericResult _ = http2_send_connection_error(
					    descriptor, context, Http2ErrorCodeInternalError, error);
					UNUSED(_);

					return (Http2ProcessFrameResult){
						.type = Http2ProcessFrameResultTypeError,
						.value = { .error = { .is_connection_error = true, .message = error } },
					};
				}
			} else {
				// TODO(Totto): should a stream in the idle state even exist?
				stream->end_stream =
				    stream->end_stream ||      // NOLINT(readability-implicit-bool-conversion)
				    headers_frame->end_stream; // NOLINT(readability-implicit-bool-conversion)

				if(headers_frame->priority_opt.has_priority) {
					stream->priority = headers_frame->priority_opt.priority;
				}

				if(stream->headers.finished) {
					const tstr_static error = TSTR_STATIC_LIT(
					    "Headers already finished but still received a headers frame");
					const GenericResult _ = http2_send_stream_error(
					    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
					UNUSED(_);

					http2_close_stream(stream);

					return (Http2ProcessFrameResult){
						.type = Http2ProcessFrameResultTypeError,
						.value = { .error = { .is_connection_error = false, .message = error } },
					};
				}
			}

			// this pushes the block, sets it in the frame to NULL, so that the frame freeing
			// doesn't clear the buffer!
			if(!http2_stream_add_header_block(stream, &(headers_frame->block_fragment),
			                                  headers_frame->end_headers)) {
				return (
				    Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
					                          .value = {
					                              .error = { .is_connection_error = true,
					                                         .message = TSTR_STATIC_LIT(
					                                             "stream header block add error") },
					                          } };
			}

			// NOTE: headers frames can end a stream, if this header frame has set the
			// end_stream and the end_headers flag!
			if(stream->end_stream &&       // NOLINT(readability-implicit-bool-conversion)
			   stream->headers.finished) { // NOLINT(readability-implicit-bool-conversion)

				// TODO(Totto): maybe set some special flag or state in the stream?
				stream->state = Http2StreamStateHalfClosed;

				return (Http2ProcessFrameResult){ .type =
					                                  Http2ProcessFrameResultTypeNewFinishedRequest,
					                              .value = {
					                                  .request =
					                                      (Http2ProcessFrameFinishedRequest){
					                                          .identifier = stream_identifier },
					                              } };
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypePriority: {
			const Http2PriorityFrame priority_frame = frame->value.priority;

			Http2Stream* stream = get_http2_stream(context, stream_identifier);

			if(stream == NULL) {

				Http2Stream new_stream = {
					.state = Http2StreamStateIdle,
					.headers = EMPTY_STREAM_HEADERS,
					.content = EMPTY_STREAM_CONTENT,
					.end_stream = false,
					.priority = priority_frame.priority,
				};

				if(!add_new_http2_stream(context, stream_identifier, new_stream)) {
					return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
						                              .value = {
						                                  .error = { .is_connection_error = true,
						                                             .message = TSTR_STATIC_LIT(
						                                                 "stream insert error") },
						                              } };
				}
			} else {
				stream->priority = priority_frame.priority;
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeRstStream: {
			const Http2RstStreamFrame rst_stream_frame = frame->value.rst_stream;

			if(rst_stream_frame.error_code != Http2ErrorCodeNoError) {
				LOG_MESSAGE(LogLevelWarn, "Received rst stream error code: %d\n",
				            rst_stream_frame.error_code);
			}

			if(stream_state == Http2StreamStateIdle) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Rst Stream frame send on a stream in an invalid state");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream_by_identifier(context, stream_identifier);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			Http2Stream* stream = get_http2_stream(context, stream_identifier);

			if(stream == NULL) {
				const tstr_static error = TSTR_STATIC_LIT("Implementation error, stream not found");
				const GenericResult _ = http2_send_connection_error(
				    descriptor, context, Http2ErrorCodeInternalError, error);
				UNUSED(_);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = true, .message = error } },
				};
			}

			if(stream->state == Http2StreamStateHalfClosed) {
				http2_close_stream(stream);
			} else {
				stream->state = Http2StreamStateHalfClosed;
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypePushPromise: {
			Http2PushPromiseFrame* push_promise_frame = &(frame->value.push_promise);

			if(stream_state != Http2StreamStateOpen && stream_state != Http2StreamStateHalfClosed) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Push promise frame send on a stream in an invalid state");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream_by_identifier(context, stream_identifier);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			Http2Stream new_stream = {
				.state = Http2StreamStateReserved,
				.headers = EMPTY_STREAM_HEADERS,
				.content = EMPTY_STREAM_CONTENT,
				.end_stream = false,
				.priority = DEFAULT_STREAM_PRIORITY_EXT(stream_identifier.identifier),
			};

			if(push_promise_frame->block_fragment.size > 0) {
				// this pushes the block, sets it in the frame to NULL, so that the frame
				// freeing doesn't clear the buffer!
				if(!http2_stream_add_header_block(&new_stream, &push_promise_frame->block_fragment,
				                                  push_promise_frame->end_headers)) {
					return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
						                              .value = {
						                                  .error = { .is_connection_error = true,
						                                             .message = TSTR_STATIC_LIT(
						                                                 "stream header "
						                                                 "block add error") },
						                              } };
				}
			}

			if(!add_new_http2_stream(context, push_promise_frame->promised_stream_identifier,
			                         new_stream)) {
				return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
					                              .value = {
					                                  .error = { .is_connection_error = true,
					                                             .message = TSTR_STATIC_LIT(
					                                                 "stream insert error") },
					                              } };
			}

			// NOTE: push promises can newer end a stream!
			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeWindowUpdate: {
			const Http2WindowUpdateFrame window_update_frame = frame->value.window_update;
			// TODO(Totto): use this frame (it is for an identifier, as window updates can be
			// also for the entire connection)
			UNUSED(window_update_frame);

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeContinuation: {
			Http2ContinuationFrame* continuation_frame = &(frame->value.continuation);

			if(stream_state != Http2StreamStateOpen && stream_state != Http2StreamStateReserved) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Continuation frame send on a stream in an invalid state");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream_by_identifier(context, stream_identifier);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			Http2Stream* stream = get_http2_stream(context, stream_identifier);

			if(stream == NULL) {
				const tstr_static error = TSTR_STATIC_LIT("Implementation error, stream not found");
				const GenericResult _ = http2_send_connection_error(
				    descriptor, context, Http2ErrorCodeInternalError, error);
				UNUSED(_);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = true, .message = error } },
				};
			}

			if(stream->headers.finished) {
				const tstr_static error = TSTR_STATIC_LIT(
				    "Headers already finished but still received a continuation frame");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream(stream);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}

			if(continuation_frame->block_fragment.size == 0) {
				const tstr_static error =
				    TSTR_STATIC_LIT("Continuation frame with empty headers block");
				const GenericResult _ = http2_send_stream_error(
				    descriptor, Http2ErrorCodeProtocolError, stream_identifier);
				UNUSED(_);

				http2_close_stream(stream);

				return (Http2ProcessFrameResult){
					.type = Http2ProcessFrameResultTypeError,
					.value = { .error = { .is_connection_error = false, .message = error } },
				};
			}
			// this pushes the block, sets it in the frame to NULL, so that the frame freeing
			// doesn't clear the buffer!
			if(!http2_stream_add_header_block(stream, &continuation_frame->block_fragment,
			                                  continuation_frame->end_headers)) {
				return (
				    Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
					                          .value = {
					                              .error = { .is_connection_error = true,
					                                         .message =
					                                             TSTR_STATIC_LIT("stream header block add error") ,},
					                          } };
			}

			// NOTE: continuation frames can end a stream, if the starting header frame has set
			// the end_stream flag and this continuation frame has the end_headers flag set!
			if(stream->end_stream &&       // NOLINT(readability-implicit-bool-conversion)
			   stream->headers.finished) { // NOLINT(readability-implicit-bool-conversion)

				// TODO(Totto): maybe set some special flag or state in the stream?
				stream->state = Http2StreamStateHalfClosed;

				return (Http2ProcessFrameResult){ .type =
					                                  Http2ProcessFrameResultTypeNewFinishedRequest,
					                              .value = {
					                                  .request =
					                                      (Http2ProcessFrameFinishedRequest){
					                                          .identifier = stream_identifier },
					                              } };
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeSettings:
		case Http2FrameTypePing:
		case Http2FrameTypeGoaway: {
			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
				                              .value = {
				                                  .error = { .is_connection_error = true,
				                                             .message = TSTR_STATIC_LIT(
				                                                 "invalid frame type to "
				                                                 "process for a stream") },
				                              } };
		}
		default: {
			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
				                              .value = {
				                                  .error = { .is_connection_error = true,
				                                             .message = TSTR_STATIC_LIT(
				                                                 "unkown frame type to process") },
				                              } };
		}
	}
}

NODISCARD static Http2ProcessFrameResult
process_http2_frame_for_connection(HTTP2Context* const context, const Http2Frame* const frame,
                                   ConnectionDescriptor* const descriptor) {
	switch(frame->type) {

		case Http2FrameTypeSettings: {
			const Http2SettingsFrame settings_frame = frame->value.settings;

			if(!settings_frame.ack) {
				http2_apply_settings_frame(context, settings_frame);
				Http2SettingsFrame frame_to_send = {
					.ack = true,
					.entries = TVEC_EMPTY(Http2SettingSingleValue),
				};
				const GenericResult _ = http2_send_settings_frame(descriptor, frame_to_send);
				UNUSED(_);
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}

		case Http2FrameTypePing: {
			const Http2PingFrame ping_frame = frame->value.ping;

			if(!ping_frame.ack) {
				Http2PingFrame frame_to_send = { .ack = true,
					                             .opaque_data = ping_frame.opaque_data };

				const GenericResult _ = http2_send_ping_frame(descriptor, frame_to_send);
				UNUSED(_);
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeGoaway: {
			const Http2GoawayFrame goaway_frame = frame->value.goaway;

			context->state.state =
			    (Http2State){ .type = Http2StateTypeClosed,
				              .value = { .closed_reason = goaway_frame.error_code } };

			if(goaway_frame.error_code != Http2ErrorCodeNoError) {

				LOG_MESSAGE(LogLevelWarn, "Received goaway error code: %d\n",
				            goaway_frame.error_code);
				if(goaway_frame.additional_debug_data.size > 0) {
					LOG_MESSAGE(LogLevelWarn, "Additional data:\n" SIZED_BUFFER_FMT "\n",
					            SIZED_BUFFER_FMT_ARGS(goaway_frame.additional_debug_data));
				}
			}

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeCloseConnection };
		}
		case Http2FrameTypeWindowUpdate: {
			const Http2WindowUpdateFrame window_update_frame = frame->value.window_update;
			UNUSED(window_update_frame);

			// TODO(Totto): use this frame (it is for the entire connection)
			UNUSED(window_update_frame);

			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeOk };
		}
		case Http2FrameTypeData:
		case Http2FrameTypeHeaders:
		case Http2FrameTypePriority:
		case Http2FrameTypeRstStream:
		case Http2FrameTypePushPromise:
		case Http2FrameTypeContinuation: {
			return (
			    Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
				                          .value = {
				                              .error = { .is_connection_error = true,
				                                         .message = TSTR_STATIC_LIT(
				                                             "invalid frame type to process for "
				                                             "a connection") },
				                          } };
		}
		default: {
			return (Http2ProcessFrameResult){ .type = Http2ProcessFrameResultTypeError,
				                              .value = {
				                                  .error = { .is_connection_error = true,
				                                             TSTR_STATIC_LIT(
				                                                 "unkown frame type to process") },
				                              } };
		}
	}
}

static void update_last_stream_id(HTTP2Context* const context, const Http2Identifier identifier) {

	if(context->state.last_stream_id.identifier < identifier.identifier) {
		context->state.last_stream_id.identifier = identifier.identifier;
	}
}

// for annotation purposes, "moves" the value from the stack inot the function, than it can be used
// in the function and freed inside that, not optimal, but required in at least one place
#define MOVED_VALUE(Type) Type* const
#define MOVE_INTO(value) &value

NODISCARD static Http2ProcessFrameResult
process_http2_frame(HTTP2Context* const context, MOVED_VALUE(Http2Frame) frame,
                    ConnectionDescriptor* const descriptor) {

	const Http2FrameCategory frame_category = get_http2_frame_category(*frame);

	const Http2Identifier stream_identifier = frame_category.value.stream_identifier;

	update_last_stream_id(context, stream_identifier);

	const Http2ProcessFrameResult process_result =
	    frame_category.is_associated_with_stream // NOLINT(readability-implicit-bool-conversion)
	        ? process_http2_frame_for_stream(stream_identifier, context, frame, descriptor)
	        : process_http2_frame_for_connection(context, frame, descriptor);

	free_http2_frame(frame);

	return process_result;
}

typedef struct {
	bool is_ok;
	SizedBuffer result;
} ConcatDataBlocksResult;

NODISCARD static ConcatDataBlocksResult http2_concat_data_blocks(const DataBlocks data_blocks) {

	size_t size = 0;

	for(size_t i = 0; i < TVEC_LENGTH(SizedBuffer, data_blocks); ++i) {
		SizedBuffer entry = TVEC_AT(SizedBuffer, data_blocks, i);

		size += entry.size;
	}

	if(size == 0) {
		return (ConcatDataBlocksResult){
			.is_ok = true,
			.result = (SizedBuffer){ .data = NULL, .size = 0 },
		};
	}

	SizedBuffer result = allocate_sized_buffer(size);

	if(result.data == NULL) {
		return (ConcatDataBlocksResult){
			.is_ok = false,
			.result = (SizedBuffer){ .data = NULL, .size = 0 },
		};
	}

	uint8_t* const result_ptr = (uint8_t*)result.data;

	size_t current_offset = 0;

	for(size_t i = 0; i < TVEC_LENGTH(SizedBuffer, data_blocks); ++i) {
		SizedBuffer entry = TVEC_AT(SizedBuffer, data_blocks, i);

		memcpy((void*)(result_ptr + current_offset), entry.data, entry.size);

		current_offset += entry.size;
	}

	return (ConcatDataBlocksResult){
		.is_ok = true,
		.result = result,
	};
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	Http2RequestHeadersResultTypeOk = 0,
	Http2RequestHeadersResultTypeError,
} Http2RequestHeadersResultType;

typedef struct {
	Http2RequestHeadersResultType type;
	union {
		HttpRequestHead result;
		const char* error;
	} data;
} Http2RequestHeadersResult;

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	PseudoHeadersForHttp2None = 0b0,
	PseudoHeadersForHttp2Method = 0b1,
	PseudoHeadersForHttp2Scheme = 0b10,
	PseudoHeadersForHttp2Authority = 0b100,
	PseudoHeadersForHttp2Path = 0b1000,
	PseudoHeadersForHttp2Status = 0b10000,
	//
	PseudoHeadersForHttp2NeededForRequest = PseudoHeadersForHttp2Method |
	                                        PseudoHeadersForHttp2Scheme | PseudoHeadersForHttp2Path,
} PseudoHeadersForHttp2;

NODISCARD static Http2RequestHeadersResult
parse_http2_headers(HpackDecompressState* const hpack_decompress_state,
                    const Http2StreamHeaders headers, const Http2Identifier stream_identifier) {

	const ConcatDataBlocksResult header_value_res = http2_concat_data_blocks(headers.header_blocks);

	if(!header_value_res.is_ok) {
		return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
			                                .data = {
			                                    .error = "error in constructing the header data",
			                                } };
	}
	const SizedBuffer header_value = header_value_res.result;

	const Http2HpackDecompressResult header_result = http2_hpack_decompress_data(
	    hpack_decompress_state, readonly_buffer_from_sized_buffer(header_value));

	free_sized_buffer(header_value);

	if(header_result.is_error) {
		return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
			                                .data = {
			                                    .error = header_result.data.error,
			                                } };
	}

	HttpHeaderFields http2_headers = header_result.data.result;

	HttpRequestHead result = {
		.request_line =
		    (HttpRequestLine){
		        .protocol_data = { .version = HTTPProtocolVersion2,
		                           .value = { .v2 = { .stream_identifier = stream_identifier } } },
		        .uri =
		            (ParsedRequestURI){
		                .type = ParsedURITypeAbsoluteURI,
		                .data = { .uri =
		                              (ParsedURI){
		                                  .authority =
		                                      (ParsedAuthority){
		                                          .host = tstr_null(),
		                                          .user_info =
		                                              (URIUserInfo){ .username = tstr_null(),
		                                                             .password = tstr_null() },
		                                          .port = 0 },

		                              } },
		            },
		    },
		.header_fields = TVEC_EMPTY(HttpHeaderField),
	};

	// see: https://datatracker.ietf.org/doc/html/rfc7540#section-8.1.2

	bool pseudo_headers_finished = false;

	PseudoHeadersForHttp2 found_pseudo_headers = PseudoHeadersForHttp2None;

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, http2_headers); ++i) {
		HttpHeaderField entry = TVEC_AT(HttpHeaderField, http2_headers, i);

		if(tstr_len(&entry.key) > 0 && tstr_cstr(&entry.key)[0] == ':') {
			if(pseudo_headers_finished) {
				return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
					                                .data = {
					                                    .error = "pseudo header field after normal "
					                                             "header fields",
					                                } };
			}

			PseudoHeadersForHttp2 new_pseudo_header = PseudoHeadersForHttp2None;

			if(tstr_eq_static_tstr(&entry.key, HTTP_HEADER_NAME(http2_pseudo_method))) {

				bool success = false;

				const HTTPRequestMethod method =
				    get_http_method_from_string(tstr_as_view(&entry.value), &success);

				if(!success) {
					return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
						                                .data = {
						                                    .error = "invalid http method",
						                                } };
				}

				result.request_line.method = method;
				new_pseudo_header = PseudoHeadersForHttp2Method;

			} else if(tstr_eq_static_tstr(&entry.key, HTTP_HEADER_NAME(http2_pseudo_scheme))) {

				assert(result.request_line.uri.type == ParsedURITypeAbsoluteURI);
				result.request_line.uri.data.uri.scheme = tstr_dup(&entry.value);
				new_pseudo_header = PseudoHeadersForHttp2Scheme;

			} else if(tstr_eq_static_tstr(&entry.key, HTTP_HEADER_NAME(http2_pseudo_authority))) {

				AuthorityResult authority_parse_result =
				    parse_authority(tstr_as_view(&entry.value));

				if(!authority_parse_result.ok) {
					return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
						                                .data = {
						                                    .error = "Authority parse error: not a "
						                                             "valid authority",
						                                } };
				}

				if(authority_parse_result.after.len != 0) {

					return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
						                                .data = {
						                                    .error = "Authority parse error: we "
						                                             "got more data "
						                                             "after the authority",
						                                } };
				}

				assert(result.request_line.uri.type == ParsedURITypeAbsoluteURI);
				result.request_line.uri.data.uri.authority = authority_parse_result.authority;
				new_pseudo_header = PseudoHeadersForHttp2Authority;

			} else if(tstr_eq_static_tstr(&entry.key, HTTP_HEADER_NAME(http2_pseudo_path))) {

				const ParsedURLPath path = parse_url_path(tstr_as_view(&entry.value));

				assert(result.request_line.uri.type == ParsedURITypeAbsoluteURI);
				result.request_line.uri.data.uri.path = path;
				new_pseudo_header = PseudoHeadersForHttp2Path;

			} else if(tstr_eq_static_tstr(&entry.key, HTTP_HEADER_NAME(http2_pseudo_status))) {
				return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
					                                .data = {
					                                    .error = "pseudo header status not allowed "
					                                             "in request",
					                                } };
			} else {
				return (
				    Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
					                            .data = {
					                                .error = "pseudo header field not recognized",
					                            } };
			}

			assert(new_pseudo_header != PseudoHeadersForHttp2None && "implementation error");

			if((found_pseudo_headers & new_pseudo_header) != 0) {
				return (
				    Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
					                            .data = {
					                                .error = "duplicate pseudo header field found",
					                            } };
			}

			found_pseudo_headers =     // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			    found_pseudo_headers | // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			    new_pseudo_header;     // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
			free_http_header_field(entry);

		} else {
			// normal header, just pass along, it is not freed, as it is reused by the new
			// header list
			pseudo_headers_finished = true;

			auto _ = TVEC_PUSH(HttpHeaderField, &(result.header_fields), entry);
			UNUSED(_);
		}
	}
	TVEC_FREE(HttpHeaderField, &http2_headers);

	if((found_pseudo_headers & PseudoHeadersForHttp2NeededForRequest) !=
	   PseudoHeadersForHttp2NeededForRequest) {
		return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeError,
			                                .data = {
			                                    .error =
			                                        "not all needed pseudo header field were found",
			                                } };
	}

	return (Http2RequestHeadersResult){ .type = Http2RequestHeadersResultTypeOk,
		                                .data = {
		                                    .result = result,
		                                } };
}

NODISCARD static HttpRequestResult
get_http2_request_from_finished_stream(Http2ContextState* const state,
                                       const Http2Stream* const stream,
                                       const Http2Identifier stream_identifier) {

	const Http2RequestHeadersResult headers_result = parse_http2_headers(
	    state->hpack_state.decompress_state, stream->headers, stream_identifier);

	if(headers_result.type != Http2RequestHeadersResultTypeOk) {
		LOG_MESSAGE(LogLevelError, "Error in headers parsing: %s\n", headers_result.data.error);
		return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = TSTR_STATIC_LIT("parse error in parsing http2 headers") ,}

				                                },
				                        } };
	}

	const HttpRequestHead head = headers_result.data.result;

	const ConcatDataBlocksResult body_res = http2_concat_data_blocks(stream->content.data_blocks);

	if(!body_res.is_ok) {
		free_http_request_head(head);

		return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = TSTR_STATIC_LIT("error in constructing the body data") ,}

				                                },
				                        } };
	}

	const HttpRequest request = { .head = head, .body = body_res.result };

	const RequestSettings settings = get_request_settings(request);

	const HTTPResultOk result_ok = {
		.request = request,
		.settings = settings,
	};

	return (HttpRequestResult){ .type = HttpRequestResultTypeOk,
		                        .value = {
		                            .ok = result_ok,
		                        } };
}

NODISCARD HttpRequestResult parse_http2_request(HTTP2Context* const context,
                                                BufferedReader* const reader) {

	while(true) {
		const Http2FrameResult frame_result = parse_http2_frame(context, reader);
		// process the frame, if possible, otherwise do it later (e.g. when the headers end)

		if(frame_result.is_error) {
			return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = frame_result.data.error ,}

				                                },
				                        } };
		}

		Http2Frame frame = frame_result.data.frame;

		// after the parsing of the frame, we can discard that data
		buffered_reader_invalidate_old_data(reader);

		const Http2ProcessFrameResult process_result = process_http2_frame(
		    context, MOVE_INTO(frame), buffered_reader_get_connection_descriptor(reader));

		switch(process_result.type) {
			case Http2ProcessFrameResultTypeError: {
				const Http2ProcessFrameError error = process_result.value.error;
				if(!error.is_connection_error) {
					// if it isn't a connection error, we can continue, it just was a stream error
					break;
				}

				return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced =  error.message,}

				                                },
				                        } };
			}
			case Http2ProcessFrameResultTypeNewFinishedRequest: {
				const Http2ProcessFrameFinishedRequest request = process_result.value.request;

				const Http2Stream* const stream = get_http2_stream(context, request.identifier);

				if(stream == NULL) {
					const tstr_static error =
					    TSTR_STATIC_LIT("Implementation error, stream not found");
					const GenericResult _ = http2_send_connection_error(
					    buffered_reader_get_connection_descriptor(reader), context,
					    Http2ErrorCodeInternalError, error);
					UNUSED(_);

					return (HttpRequestResult){ .type = HttpRequestResultTypeError,
						                        .value = {
						                            .error =
						                                (HttpRequestError){
						                                    .is_advanced = true,
						                                    .value = { .advanced = error }

						                                },
						                        } };
				}

				return get_http2_request_from_finished_stream((&context->state), stream,
				                                              request.identifier);
			}
			case Http2ProcessFrameResultTypeOk: {
				break;
			}
			case Http2ProcessFrameResultTypeCloseConnection: {
				return (HttpRequestResult){
					.type = HttpRequestResultTypeCloseConnection,
					.value = {},
				};
			}
			default: {
				return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = TSTR_STATIC_LIT("invalid process frame result type") ,}

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

	if(request_line.protocol_data.version != HTTPProtocolVersion2) {
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

	if(sized_buffer_eq_data(res.value.buffer, HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE,
	                        SIZEOF_HTTP2_CLIENT_PREFACE_AFTER_HTTP1_STATUS_LINE)) {

		return Http2PrefaceStatusOk;
	}

	return Http2PrefaceStatusErr;
}

NODISCARD static Http2SettingsFrame
get_start_settings_frame(Http2ContextState* const context_state) {

	// TODO(Totto): select the best settings, we should send
	Http2SettingsFrame settings_frame = {
		.ack = false,
		.entries = TVEC_EMPTY(Http2SettingSingleValue),
	};

	const size_t hpack_dynamic_table_max_size = DEFAULT_HEADER_TABLE_SIZE;

	{
		const Http2SettingSingleValue dynamic_table_value = {
			.identifier = Http2SettingsFrameIdentifierHeaderTableSize,
			.value = hpack_dynamic_table_max_size,
		};

		auto _ = TVEC_PUSH(Http2SettingSingleValue, &settings_frame.entries, dynamic_table_value);
		UNUSED(_);
	}

	set_hpack_compress_state_setting(context_state->hpack_state.compress_state,
	                                 hpack_dynamic_table_max_size);

	return settings_frame;
}

NODISCARD Http2StartResult http2_send_and_receive_preface(HTTP2Context* const context,
                                                          BufferedReader* const reader) {

	const Http2SettingsFrame frame_to_send = get_start_settings_frame(&(context->state));

	const GenericResult result =
	    http2_send_settings_frame(buffered_reader_get_connection_descriptor(reader), frame_to_send);

	free_http2_settings_frame(frame_to_send);

	IF_GENERIC_RESULT_IS_ERROR_IGN(result) {
		return (Http2StartResult){
			.is_error = true,
			.value = { .error =
			               TSTR_STATIC_LIT("error in sending settings frame (server preface)") }
		};
	}

	Http2FrameResult frame_result = parse_http2_frame(context, reader);

	if(frame_result.is_error) {
		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = frame_result.data.error } };
	}

	Http2Frame frame = frame_result.data.frame;

	if(frame.type != Http2FrameTypeSettings) {
		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = TSTR_STATIC_LIT(
			                                      "first frame has to be a settings frame") } };
	}

	// after the parsing of the frame, we can discard that data
	buffered_reader_invalidate_old_data(reader);

	const Http2ProcessFrameResult process_result = process_http2_frame(
	    context, MOVE_INTO(frame), buffered_reader_get_connection_descriptor(reader));

	if(process_result.type != Http2ProcessFrameResultTypeOk) {
		const tstr_static error =
		    (process_result.type == Http2ProcessFrameResultTypeError)
		        ? process_result.value.error.message
		        : TSTR_STATIC_LIT(
		              "error: implementation error (settings frame can't result in a request)");
		return (Http2StartResult){ .is_error = true, .value = { .error = error } };
	}

	return (Http2StartResult){ .is_error = false };
}

NODISCARD static Http2StartResult http2_receive_preface_with_magic(HTTP2Context* const context,
                                                                   BufferedReader* const reader) {

	// get magic first, after that the client can send a settings frame
	BufferedReadResult read_result =
	    buffered_reader_get_amount(reader, SIZEOF_HTTP2_CLIENT_PREFACE);

	if(read_result.type != BufferedReadResultTypeOk) {

		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = TSTR_STATIC_LIT(
			                                      "unable to read magic http2 preface") } };
	}

	const SizedBuffer buffer = read_result.value.buffer;

	if(!sized_buffer_eq_data(buffer, HTTP2_CLIENT_PREFACE, SIZEOF_HTTP2_CLIENT_PREFACE)) {

		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = TSTR_STATIC_LIT(
			                                      "magic http2 preface not correct") } };
	}

	Http2FrameResult frame_result = parse_http2_frame(context, reader);

	if(frame_result.is_error) {
		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = frame_result.data.error } };
	}

	Http2Frame frame = frame_result.data.frame;

	if(frame.type != Http2FrameTypeSettings) {
		return (Http2StartResult){ .is_error = true,
			                       .value = { .error = TSTR_STATIC_LIT(
			                                      "first frame has to be a settings frame") } };
	}

	// after the parsing of the frame, we can discard that data
	buffered_reader_invalidate_old_data(reader);

	const Http2ProcessFrameResult process_result = process_http2_frame(
	    context, MOVE_INTO(frame), buffered_reader_get_connection_descriptor(reader));

	if(process_result.type != Http2ProcessFrameResultTypeOk) {
		const tstr_static error =
		    process_result.type == Http2ProcessFrameResultTypeError
		        ? process_result.value.error.message
		        : TSTR_STATIC_LIT(
		              "error: implementation error (settings frame can't result in a request)");
		return (Http2StartResult){ .is_error = true, .value = { .error = error } };
	}

	return (Http2StartResult){ .is_error = false };
}

NODISCARD HttpRequestResult http2_process_h2c_upgrade(HTTP2Context* const context,
                                                      BufferedReader* const reader,
                                                      const SizedBuffer settings_data,
                                                      const HttpRequest original_request) {

	// send settings frame first:
	const Http2SettingsFrame frame_to_send = get_start_settings_frame(&(context->state));

	const GenericResult result =
	    http2_send_settings_frame(buffered_reader_get_connection_descriptor(reader), frame_to_send);

	free_http2_settings_frame(frame_to_send);

	IF_GENERIC_RESULT_IS_ERROR_IGN(result) {
		free_sized_buffer(settings_data);

		return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = TSTR_STATIC_LIT("error in sending settings frame (server preface)") ,}

				                                },
				                        } };
	}

	const Http2FrameResult settings_frame_result =
	    parse_raw_http2_settings_frame(settings_data, reader, context, false);

	free_sized_buffer(settings_data); // NOLINT(clang-analyzer-unix.Malloc)

	if(settings_frame_result.is_error) {
		return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = settings_frame_result.data.error ,}

				                                },
				                        } };
	}

	const Http2Frame frame = settings_frame_result.data.frame;

	// apply settings

	if(frame.type != Http2FrameTypeSettings) {

		free_http2_frame(&frame);

		return (HttpRequestResult){ .type = HttpRequestResultTypeError,
				                        .value = {
				                            .error =
				                                (HttpRequestError){
				                                    .is_advanced = true,
				                                    .value = { .advanced = TSTR_STATIC_LIT("error: implementation error parsing settings frame didn't result in one"),}

				                                },
				                        } };
	}

	http2_apply_settings_frame(context, frame.value.settings);
	free_http2_settings_frame(frame.value.settings);

	const Http2StartResult start_result = http2_receive_preface_with_magic(context, reader);

	if(start_result.is_error) {
		return (HttpRequestResult){
			.type = HttpRequestResultTypeError,
			.value = { .error =
			               (HttpRequestError){
			                   .is_advanced = false,
			                   .value = { .enum_value =
			                                  HttpRequestErrorTypeInvalidHttp2Preface } } }
		};
	}

	HttpRequest new_http2_request = original_request;

	// see: https://datatracker.ietf.org/doc/html/rfc7540#section-3.2

	Http2Identifier stream_identifier = { .identifier = 1 };

	Http2Stream new_stream = {
		.state = Http2StreamStateHalfClosed,
		.headers = EMPTY_STREAM_HEADERS,
		.content = EMPTY_STREAM_CONTENT,
		.end_stream = true,
		.priority = DEFAULT_STREAM_PRIORITY,
	};

	if(!add_new_http2_stream(context, stream_identifier, new_stream)) {
		return (HttpRequestResult){ .type = HttpRequestResultTypeError,
			                        .value = { .error = (HttpRequestError){
			                                       .is_advanced = true,
			                                       .value = { .advanced = TSTR_STATIC_LIT(
			                                                      "stream insert error") } } } };
	}

	new_http2_request.head.request_line.protocol_data =
	    (HttpProtocolData){ .version = HTTPProtocolVersion2,
		                    .value = { .v2 = { .stream_identifier = stream_identifier } } };

	const RequestSettings settings = get_request_settings(new_http2_request);

	const HTTPResultOk result_ok = {
		.request = new_http2_request,
		.settings = settings,
	};

	return (HttpRequestResult){ .type = HttpRequestResultTypeOk,
		                        .value = {
		                            .ok = result_ok,
		                        } };
}

static void free_http2_streams(Http2StreamMap streams) {

	TMAP_TYPENAME_ITER(Http2StreamMap)
	iter = TMAP_ITER_INIT(Http2StreamMap, &streams);

	TMAP_TYPENAME_ENTRY(Http2StreamMap) value;

	while(TMAP_ITER_NEXT(Http2StreamMap, &iter, &value)) {
		free_http2_stream(&value.value);
	}

	TMAP_FREE(Http2StreamMap, &streams);
}

static void free_hpack_state(Http2HpackState state) {
	free_hpack_decompress_state(state.decompress_state);
	free_hpack_compress_state(state.compress_state);
}

static void free_http2_context_state(Http2ContextState state) {
	free_hpack_state(state.hpack_state);
}

void free_http2_context(HTTP2Context context) {

	free_http2_context_state(context.state);
	free_http2_streams(context.streams);
}

#define ADDITIONAL_HEADERS_DATA_SIZE ((8 + 32 + 8) / 8)

NODISCARD static size_t get_max_headers_or_continuation_contentsize(Http2Settings settings) {

	const size_t max_frame_size = settings.max_frame_size;

	assert(max_frame_size > ADDITIONAL_HEADERS_DATA_SIZE);

	const size_t max_headers_size = max_frame_size - ADDITIONAL_HEADERS_DATA_SIZE;

	// the continuation frame has no other overhead, than the payload

	return max_headers_size;
}

NODISCARD static size_t get_max_data_content_size(Http2Settings settings) {
	const size_t max_frame_size = settings.max_frame_size;

	// the data frame has no other overhead, than the payload, if no padding is used

	return max_frame_size;
}

NODISCARD GenericResult http2_send_headers(const ConnectionDescriptor* descriptor,
                                           Http2Identifier identifier, Http2Settings settings,
                                           SizedBuffer buffer, const bool headers_are_end_stream) {

	const size_t max_header_payload_size = get_max_headers_or_continuation_contentsize(settings);

	for(size_t offset = 0; offset < buffer.size;) {

		const size_t size = ((offset + max_header_payload_size) >= buffer.size)
		                        ? buffer.size - offset
		                        : max_header_payload_size;

		SizedBuffer block_fragment = {
			.data = ((uint8_t*)buffer.data) + offset,
			.size = size,
		};

		const bool end_headers = offset + size >= buffer.size;

		if(offset == 0) {
			Http2HeadersFrame frame = {
				.priority_opt = { .has_priority = false },
				.end_headers = end_headers,
				.end_stream = headers_are_end_stream,
				.block_fragment = block_fragment,
				.identifier = identifier,
			};
			const GenericResult result = http2_send_headers_frame(descriptor, frame);

			IF_GENERIC_RESULT_IS_ERROR_IGN(result) {
				return result;
			}
		} else {
			Http2ContinuationFrame frame = {
				.end_headers = end_headers,
				.block_fragment = block_fragment,
				.identifier = identifier,
			};
			const GenericResult result = http2_send_continuation_frame(descriptor, frame);
			IF_GENERIC_RESULT_IS_ERROR_IGN(result) {
				return result;
			}
		}

		offset += size;
	}

	return GENERIC_RES_OK();
}

NODISCARD GenericResult http2_send_data(const ConnectionDescriptor* descriptor,
                                        Http2Identifier identifier, Http2Settings settings,
                                        const SizedBuffer buffer) {

	const size_t max_data_payload_size = get_max_data_content_size(settings);

	for(size_t offset = 0; offset < buffer.size;) {

		const size_t size = ((offset + max_data_payload_size) >= buffer.size)
		                        ? buffer.size - offset
		                        : max_data_payload_size;

		const SizedBuffer content = {
			.data = ((uint8_t*)buffer.data) + offset,
			.size = size,
		};

		const bool is_end = offset + size >= buffer.size;

		Http2DataFrame frame = {
			.content = content,
			.identifier = identifier,
			.is_end = is_end,
		};
		const GenericResult result = http2_send_data_frame(descriptor, frame);
		IF_GENERIC_RESULT_IS_ERROR_IGN(result) {
			return result;
		}
		offset += size;
	}

	return GENERIC_RES_OK();
}
