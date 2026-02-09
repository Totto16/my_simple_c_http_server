

#pragma once

#include "./sized_buffer.h"
#include "generic/secure.h"

typedef struct BufferedReaderImpl BufferedReader;

typedef uint8_t Byte;

BufferedReader* get_buffered_reader(ConnectionDescriptor* descriptor);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	BufferedReadResultTypeOk = 0,
	BufferedReadResultTypeErr,
	BufferedReadResultTypeEOF,
} BufferedReadResultType;

typedef struct {
	BufferedReadResultType type;
	union {
		SizedBuffer data;
		const char* error;
	} value;
} BufferedReadResult;

/**
 * @brief Get the until delimiter fixed object
 *
 * @param state
 * @param delimiter
 * @return ReadResult
 */
NODISCARD BufferedReadResult buffered_reader_get_until_delimiter(BufferedReader* reader,
                                                                 const char* delimiter);

/**
 * @brief Get the until delimiter fixed object
 *
 * @param state
 * @param delimiter
 * @return BufferedReadResult
 */
NODISCARD BufferedReadResult buffered_reader_get_until_delimiter_fixed(BufferedReader* reader,
                                                                       SizedBuffer delimiter);

NODISCARD BufferedReadResult buffered_reader_get_until_end(BufferedReader* reader);

NODISCARD BufferedReadResult buffered_reader_get_amount(BufferedReader* reader, size_t amount);

/**
 * @brief This invalidates old data, freeing the large data it holds buffered
 * this can be done e.g. after parsing one http request
 *
 * @param reader
 */
void buffered_reader_invalidate_old_data(BufferedReader* reader);

NODISCARD bool buffered_reader_has_more_data(const BufferedReader* reader);

void free_buffered_reader(BufferedReader* reader);

NODISCARD bool finish_buffered_reader(BufferedReader* reader, ConnectionContext* context,
                                      bool allow_reuse);

NODISCARD ConnectionDescriptor* buffered_reader_get_connection_descriptor(BufferedReader* reader);

NODISCARD const ConnectionDescriptor* buffered_reader_get_connection_descriptor_const(const BufferedReader* reader);
