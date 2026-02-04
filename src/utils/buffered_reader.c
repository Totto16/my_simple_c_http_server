

#include "./buffered_reader.h"

typedef struct {
	size_t cursor;
	SizedBuffer data;
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
		                                                .data = (SizedBuffer){
		                                                    .data = NULL,
		                                                    .size = 0,
		                                                } } };

	return reader;
}

static bool buffered_reader_is_safe_to_read(BufferedReader* const reader) {
	if(reader->state != StreamStateOpen) {
		return false;
	}

	if(reader->data.cursor > reader->data.data.size) {
		reader->state = StreamStateError;
		return false;
	}

	return true;
}

#define BUFFERED_READER_CHUNK_SIZE 512

static void buffered_reader_get_more_data_exact(BufferedReader* const reader, size_t amount) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return;
	}

	void* new_buffer = realloc(reader->data.data.data, reader->data.data.size + amount);

	if(!new_buffer) {
		reader->state = StreamStateError;
		return;
	}

	void* buffer = (Byte*)new_buffer + reader->data.data.size;

	reader->data.data.data = new_buffer;
	reader->data.data.size += amount;

	ReadResult res = read_from_descriptor(reader->descriptor, buffer, amount);

	if(res.type == ReadResultTypeEOF) {
		reader->state = StreamStateClosed;
		return;
	}

	if(res.type == ReadResultTypeError) {
		reader->state = StreamStateError;
		return;
	}

	const size_t read_data = res.data.bytes_read;

	if(read_data != amount) {
		reader->state = StreamStateError;
		return;
	}

	reader->state = StreamStateOpen;
}

NODISCARD static size_t get_available_data_length(BufferedReader* const reader) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return 0;
	}

	return reader->data.data.size - reader->data.cursor;
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
		if(reader->data.cursor < reader->data.data.size) {
			buffered_reader_get_more_data_exact(reader, BUFFERED_READER_CHUNK_SIZE);

			if(reader->state != StreamStateOpen) {
				return (BufferedReadResult){
					.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
					                                           : BufferedReadResultTypeErr,
					.value = { .error = "Failed to get more data in read until delimiter" }
				};
			}
		}

		const Byte data_byte = ((Byte*)reader->data.data.data)[reader->data.cursor];

		const Byte delimiter_byte = delimiter_bytes[delimiter_index];

		if(data_byte == delimiter_byte) {
			delimiter_index++;

			if(delimiter_index >= delimiter.size) {

				++reader->data.cursor;

				SizedBuffer data = {
					.data = (Byte*)reader->data.data.data + start_cursor,
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

	SizedBuffer data = {
		.data = (Byte*)reader->data.data.data + start_cursor,
		.size = (reader->data.cursor - start_cursor),
	};

	reader->data.cursor = reader->data.data.size;
	reader->state = StreamStateClosed;

	return (BufferedReadResult){ .type = BufferedReadResultTypeOk, .value = { .data = data } };
}

NODISCARD BufferedReadResult buffered_reader_get_amount(BufferedReader* const reader,
                                                        const size_t amount) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return (BufferedReadResult){
			.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
			                                           : BufferedReadResultTypeErr,
			.value = { .error = "Failed to get more data in read amount" }
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
			return (BufferedReadResult){
				.type = reader->state == StreamStateClosed ? BufferedReadResultTypeEOF
				                                           : BufferedReadResultTypeErr,
				.value = { .error = "Failed to get more data in read amount" }
			};
		}
	}

	const size_t size = (reader->data.cursor - start_cursor);
	assert(size == amount);

	SizedBuffer data = {
		.data = (Byte*)reader->data.data.data + start_cursor,
		.size = size,
	};

	reader->data.cursor = reader->data.data.size;
	reader->state = StreamStateClosed;

	return (BufferedReadResult){ .type = BufferedReadResultTypeOk, .value = { .data = data } };
}

void buffered_reader_invalidate_old_data(BufferedReader* const reader) {

	if(!buffered_reader_is_safe_to_read(reader)) {
		return;
	}

	void* current_data = reader->data.data.data;

	const size_t offset = reader->data.cursor;

	const size_t available_length = get_available_data_length(reader);

	if(available_length == 0) {
		free(current_data);
		reader->data.data = (SizedBuffer){ .data = NULL, .size = 0 };
		reader->data.cursor = 0;
		return;
	}

	void* new_data = malloc(available_length);

	if(!new_data) {
		return;
	}

	memcpy(new_data, (Byte*)current_data + offset, available_length);

	free(current_data);

	reader->data.data = (SizedBuffer){ .data = new_data, .size = available_length };
	reader->data.cursor = 0;
}

void free_buffered_reader(BufferedReader* const reader) {
	free_sized_buffer(reader->data.data);
}
