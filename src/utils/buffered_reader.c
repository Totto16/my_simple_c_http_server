

#include "./buffered_reader.h"
#include "utils/log.h"

typedef struct {
	size_t cursor;
	SizedBuffer buffer;
} BufferedData;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	StreamStateOpen = 0,
	StreamStateClosed,
	StreamStateError,
} StreamState;

struct BufferedReaderImpl {
	ConnectionDescriptor* descriptor;
	StreamState state;
	BufferedData data;
};

// TODO: this shopuld use non blockign reads internally everyhwere, but how to handle waits and
// where? NOTE: creation should pass a timer of some sort and an interval, so that internally we
// wait for that long, if we don't have data, and error with timeout is returned

BufferedReader* get_buffered_reader(ConnectionDescriptor* descriptor) {

	BufferedReader* reader = malloc(sizeof(BufferedReader));

	if(reader == NULL) {
		return NULL;
	}

	*reader = (BufferedReader){ .descriptor = descriptor,
		                        .state = StreamStateOpen,
		                        .data = (BufferedData){ .cursor = 0,
		                                                .buffer = (SizedBuffer){
		                                                    .data = NULL,
		                                                    .size = 0,
		                                                } } };

	return reader;
}

static bool buffered_reader_is_safe_to_read(BufferedReader* const reader) {
	if(reader->state != StreamStateOpen) {
		return false;
	}

	if(reader->data.cursor > reader->data.buffer.size) {
		reader->state = StreamStateError;
		return false;
	}

	return true;
}

#define BUFFERED_READER_CHUNK_SIZE 512

static size_t buffered_reader_get_more_data_partially(BufferedReader* const reader, size_t amount) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return 0;
	}

	void* new_buffer = realloc(reader->data.buffer.data, reader->data.buffer.size + amount);

	if(new_buffer == NULL) {
		reader->state = StreamStateError;
		return 0;
	}

	void* buffer = (Byte*)new_buffer + reader->data.buffer.size;

	reader->data.buffer.data = new_buffer;

	ReadResult res = read_from_descriptor(reader->descriptor, buffer, amount);

	if(res.type == ReadResultTypeEOF) {
		reader->state = StreamStateClosed;
		return 0;
	}

	if(res.type == ReadResultTypeError) {
		reader->state = StreamStateError;
		return 0;
	}

	reader->state = StreamStateOpen;

	const size_t bytes_read = res.data.bytes_read;

	reader->data.buffer.size += bytes_read;
	return bytes_read;
}

static void buffered_reader_get_more_data_exact(BufferedReader* const reader, const size_t amount) {

	size_t amount_left = amount;

	while(reader->state == StreamStateOpen) {

		const size_t data_read = buffered_reader_get_more_data_partially(reader, amount_left);
		if(data_read > amount_left) {
			reader->state = StreamStateError;
			return;
		}

		if(data_read == amount_left) {
			return;
		}

		amount_left -= data_read;
	}
}

static void buffered_reader_get_more_data_at_least_some(BufferedReader* const reader,
                                                        size_t amount) {
	const size_t data_read = buffered_reader_get_more_data_partially(reader, amount);

	if(reader->state == StreamStateOpen && data_read < 1) {
		reader->state = StreamStateError;
		return;
	}
}

NODISCARD static size_t get_available_data_length(BufferedReader* const reader) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return 0;
	}

	return reader->data.buffer.size - reader->data.cursor;
}

static void buffered_reader_get_data_until(BufferedReader* const reader, size_t amount) {

	const size_t current_amount = get_available_data_length(reader);

	if(current_amount >= amount) {
		return;
	}

	const size_t needs_amount = amount - current_amount;

	buffered_reader_get_more_data_exact(reader, needs_amount);
}

NODISCARD static BufferedReadResult
buffered_reader_get_until_delimiter_impl(BufferedReader* const reader,
                                         const SizedBuffer delimiter) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return (BufferedReadResult){
			.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
			                                           : BufferedReadResultTypeErr,
			.value = { .error = "Failed to get more data in read until delimiter" }
		};
	}

	size_t delimiter_index = 0;
	Byte* delimiter_bytes = (Byte*)delimiter.data;

	const size_t start_cursor = reader->data.cursor;

	while(true) {
		if(reader->data.cursor >= reader->data.buffer.size) {
			buffered_reader_get_more_data_at_least_some(reader, BUFFERED_READER_CHUNK_SIZE);

			if(reader->state != StreamStateOpen) {
				return (BufferedReadResult){
					.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
					                                           : BufferedReadResultTypeErr,
					.value = { .error = "Failed to get more data in read until delimiter" }
				};
			}
		}

		const Byte data_byte = ((Byte*)reader->data.buffer.data)[reader->data.cursor];

		const Byte delimiter_byte = delimiter_bytes[delimiter_index];

		if(data_byte == delimiter_byte) {
			delimiter_index++;

			if(delimiter_index >= delimiter.size) {

				++reader->data.cursor;

				SizedBuffer data = {
					.data = (Byte*)reader->data.buffer.data + start_cursor,
					.size = (reader->data.cursor - start_cursor - delimiter.size),
				};

				return (BufferedReadResult){ .type = BufferedReadResultTypeOk,
					                         .value = { .data = data } };
			}
		} else {
			delimiter_index = 0;
		}

		++reader->data.cursor;
	}
}

NODISCARD BufferedReadResult buffered_reader_get_until_delimiter(BufferedReader* const reader,
                                                                 const char* const delimiter) {
	SizedBuffer fixed = { .data = (void*)delimiter, .size = strlen(delimiter) };
	return buffered_reader_get_until_delimiter_impl(reader, fixed);
}

NODISCARD BufferedReadResult buffered_reader_get_until_delimiter_fixed(
    BufferedReader* const reader, const SizedBuffer delimiter) {
	return buffered_reader_get_until_delimiter_impl(reader, delimiter);
}

NODISCARD BufferedReadResult buffered_reader_get_until_end(BufferedReader* const reader) {

	if(!buffered_reader_is_safe_to_read(reader)) {

		if(reader->state == StreamStateClosed) {

			SizedBuffer data = {
				.data = NULL,
				.size = 0,
			};

			return (BufferedReadResult){ .type = BufferedReadResultTypeOk,
				                         .value = { .data = data } };
		}

		return (BufferedReadResult){
			.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
			                                           : BufferedReadResultTypeErr,
			.value = { .error = "Failed to get more data in read until end" }
		};
	}

	const size_t start_cursor = reader->data.cursor;

	while(true) {
		buffered_reader_get_more_data_exact(reader, BUFFERED_READER_CHUNK_SIZE);

		switch(reader->state) {
			case StreamStateOpen: {
				break;
			}
			case StreamStateClosed: {
				goto break_while_outer;
			}
			case StreamStateError:
			default: {
				return (BufferedReadResult){
					.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
					                                           : BufferedReadResultTypeErr,
					.value = { .error = "Failed to get more data in read until end" }
				};
			}
		}
	}

break_while_outer:

	UNUSED(start_cursor);
	assert(reader->data.cursor == start_cursor && "check if old wrong behaviour is fixed");

	SizedBuffer data = {
		.data = (Byte*)reader->data.buffer.data + reader->data.cursor,
		.size = (reader->data.buffer.size - reader->data.cursor),
	};

	reader->data.cursor = reader->data.buffer.size;
	reader->state = StreamStateClosed;

	return (BufferedReadResult){ .type = BufferedReadResultTypeOk, .value = { .data = data } };
}

NODISCARD BufferedReadResult buffered_reader_get_amount(BufferedReader* const reader,
                                                        const size_t amount) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return (BufferedReadResult){
			.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
			                                           : BufferedReadResultTypeErr,
			.value = { .error = "Failed to get more data in read amount at start" }
		};
	}

	const size_t start_cursor = reader->data.cursor;

	buffered_reader_get_data_until(reader, amount);

	switch(reader->state) {
		case StreamStateOpen: {
			break;
		}
		case StreamStateClosed:
		case StreamStateError:
		default: {
			return (BufferedReadResult){ .type = reader->state == StreamStateClosed
				                                     ? BufferedReadResultTypeEOF
				                                     : BufferedReadResultTypeErr,
				                         .value = { .error = "Failed to get more data in read "
				                                             "amount, stream not open anymore" } };
		}
	}

	UNUSED(start_cursor);
	assert(reader->data.cursor == start_cursor && "check if old wrong behaviour is fixed");
	const size_t size = get_available_data_length(reader);

	if(size < amount) {
		return (
		    BufferedReadResult){ .type = BufferedReadResultTypeErr,
			                     .value = { .error = "Failed to get more data in get amount" } };
	}

	SizedBuffer data = {
		.data = (Byte*)reader->data.buffer.data + reader->data.cursor,
		.size = amount,
	};

	reader->data.cursor += amount;

	return (BufferedReadResult){ .type = BufferedReadResultTypeOk, .value = { .data = data } };
}

void buffered_reader_invalidate_old_data(BufferedReader* const reader) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return;
	}

	void* current_data = reader->data.buffer.data;

	const size_t offset = reader->data.cursor;

	const size_t available_length = get_available_data_length(reader);

	if(available_length == 0) {
		free(current_data);
		reader->data.buffer = (SizedBuffer){ .data = NULL, .size = 0 };
		reader->data.cursor = 0;
		return;
	}

	void* new_data = malloc(available_length);

	if(!new_data) {
		return;
	}

	memcpy(new_data, (Byte*)current_data + offset, available_length);

	free(current_data);

	reader->data.buffer = (SizedBuffer){ .data = new_data, .size = available_length };
	reader->data.cursor = 0;
}

NODISCARD bool buffered_reader_has_more_data(const BufferedReader* const reader) {

	if(reader->state == StreamStateClosed) {
		return true;
	}

	if(reader->data.cursor < reader->data.buffer.size) {
		return false;
	}

	return true;
}

void free_buffered_reader(BufferedReader* const reader) {
	free_sized_buffer(reader->data.buffer);
	free(reader);
}

bool finish_buffered_reader(BufferedReader* const reader, ConnectionContext* const context,
                            bool allow_reuse) {

	// the reader was reset, here we need nothing to be free or closed
	if(reader == NULL) {
		return true;
	}

	// TODO(Totto): maybe half close the tcp connection and check if more data is given, that would
	// be a client error!

	int result = close_connection_descriptor_advanced(reader->descriptor, context, allow_reuse);
	CHECK_FOR_ERROR(result, "While trying to close the connection descriptor", { return false; });

	free_buffered_reader(reader);

	return true;
}

NODISCARD ConnectionDescriptor*
buffered_reader_get_connection_descriptor(BufferedReader* const reader) {
	return reader->descriptor;
}

NODISCARD const ConnectionDescriptor*
buffered_reader_get_connection_descriptor_const(const BufferedReader* const reader) {
	return reader->descriptor;
}
