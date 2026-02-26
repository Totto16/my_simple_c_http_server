#include "./hpack.h"
#include "./dynamic_hpack_table.h"
#include "./hpack_huffman.h"

#include "generated_hpack.h"

// see: https://datatracker.ietf.org/doc/html/rfc7541

struct HpackStateImpl {
	HpackHeaderDynamicTable dynamic_table;
	size_t max_dynamic_table_byte_size;
	size_t current_dynamic_table_byte_size;
};

// this is chosen, so that a HpackVariableInteger value can be encoded, as the max bits per byte,
// that can be used is 7, it calculated like that
#define MAX_HPACK_VARIABLE_INTEGER_SIZE (((sizeof(HpackVariableInteger) * 8) + 7) / 7)

NODISCARD HpackVariableIntegerResult decode_hpack_variable_integer(size_t* pos, const size_t size,
                                                                   const uint8_t* const data,
                                                                   uint8_t prefix_bits) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-5.1

	const uint8_t mask = (1 << prefix_bits) - 1;
	const uint8_t first_byte = (data[*pos]) & mask;

	if(first_byte < mask) {
		(*pos)++;
		return (HpackVariableIntegerResult){
			.is_error = false, .data = { .value = (HpackVariableInteger)first_byte }
		};
	}

	(*pos)++;
	uint8_t amount = 0;

	HpackVariableInteger result = first_byte;

	while(true) {
		if((*pos) >= size) {
			// not more bytes available
			return (HpackVariableIntegerResult){ .is_error = true,
				                                 .data = { .error = "not enough bytes" } };
		}

		const uint8_t byte = data[*pos];
		(*pos)++;

		if((amount / 8) >= sizeof(HpackVariableInteger) - 1) {
			// to many bytes, the index should not be larger than a uint64_t
			return (HpackVariableIntegerResult){
				.is_error = true, .data = { .error = "final integer would be too big" }
			};
		}

		result += (byte & 0x7F) << amount;
		if((byte & 0x80) == 0) {
			// this was the last byte
			break;
		}
		amount += 7;
	}

	return (HpackVariableIntegerResult){ .is_error = false,
		                                 .data = {
		                                     .value = result,
		                                 } };
}

// note: out_bytes has to have a available size of MAX_HPACK_VARIABLE_INTEGER_SIZE
NODISCARD static int8_t encode_hpack_variable_integer(uint8_t* const out_bytes,
                                                      const HpackVariableInteger input,
                                                      const uint8_t prefix_bits) {

	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-5.1

	const uint8_t mask = (1 << prefix_bits) - 1;

	if(input < mask) {
		// it can be done in one value
		out_bytes[0] = out_bytes[0] | (input & mask);
		return 1;
	}

	// it needs more bytes

	HpackVariableInteger value = input;
	int8_t result = 0;

	out_bytes[result] = out_bytes[result] | mask;

	value -= mask;
	++result;

	// first byte was special, now loop

	while(true) {
		if(result > (int)(MAX_HPACK_VARIABLE_INTEGER_SIZE)) {
			return -1;
		}

		if(value >= 0x80) {
			// not the end

			const uint8_t to_encode = value & 0x7F;
			out_bytes[result++] = 0x80 + to_encode;
			value /= 0x80;
		} else {
			out_bytes[result++] = value;
			break;
		}
	}

	return result;
}

typedef struct {
	bool is_error;
	HpackHeaderDynamicEntry value;
} HpackHeaderEntryResult;

typedef struct {
	HpackHeaderStaticEntry* static_header_table;
} GlobalHpackData;

GlobalHpackData g_hpack_static_data = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    { .static_header_table = NULL };

static void global_initialize_hpack_static_header_table_data() {
	g_hpack_static_data.static_header_table = get_hpack_static_header_table_entries();
}

static void global_free_hpack_static_header_table_data() {
	free_hpack_static_header_table_entries(g_hpack_static_data.static_header_table);
}

NODISCARD static char* strdup_null_agnostic(const char* const value) {
	if(value == NULL) {
		return NULL;
	}

	return strdup(value);
}

NODISCARD static HpackHeaderEntryResult hpack_get_table_entry_at(const HpackState* const state,
                                                                 size_t value) {

	if(value == 0) {
		return (HpackHeaderEntryResult){ .is_error = true };
	}

	if(value <= HPACK_STATIC_HEADER_TABLE_SIZE) {
		assert(g_hpack_static_data.static_header_table != NULL);

		HpackHeaderStaticEntry static_entry = g_hpack_static_data.static_header_table[value - 1];

		return (HpackHeaderEntryResult){ .is_error = false,
			                             .value = (HpackHeaderDynamicEntry){
			                                 .key = strdup(static_entry.key),
			                                 .value = strdup_null_agnostic(static_entry.value) } };
	}

	const size_t dynamic_index = value - HPACK_STATIC_HEADER_TABLE_SIZE - 1;

	const size_t dynamic_table_size = state->dynamic_table.count;

	if(dynamic_table_size <= dynamic_index) {
		return (HpackHeaderEntryResult){ .is_error = true };
	}

	const HpackHeaderDynamicEntry dynamic_entry =
	    hpack_dynamic_table_at(&(state->dynamic_table), dynamic_index);

	return (HpackHeaderEntryResult){ .is_error = false,
		                             .value = (HpackHeaderDynamicEntry){
		                                 .key = strdup(dynamic_entry.key),
		                                 .value = strdup_null_agnostic(dynamic_entry.value) } };
}

NODISCARD static int parse_hpack_indexed_header_field(size_t* pos, const size_t size,
                                                      const uint8_t* const data,
                                                      HttpHeaderFields* const headers,
                                                      const HpackState* const state) {
	// Indexed Header Field:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
	//    0   1   2   3   4   5   6   7
	//  +---+---+---+---+---+---+---+---+
	//  | 1 |        Index (7+)         |
	//  +---+---------------------------+

	const HpackVariableIntegerResult index_res = decode_hpack_variable_integer(pos, size, data, 7);

	if(index_res.is_error) {
		return -1;
	}

	const HpackVariableInteger index = index_res.data.value;

	if(index == 0) {
		return -2;
	}

	const HpackHeaderEntryResult entry = hpack_get_table_entry_at(state, index);

	if(entry.is_error) {
		return -3;
	}

	const HttpHeaderField entry_value = { .key = tstr_own_cstr(entry.value.key),
		                                  .value = tstr_own_cstr(entry.value.value) };

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, entry_value);

	if(insert_result != TvecResultOk) {
		free_dynamic_entry(entry.value);
		return -3;
	}

	return 0;
}

NODISCARD static char* parse_literal_string_value(size_t* pos, const size_t size,
                                                  const uint8_t* const data) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-5.2

	if(*pos >= size) {
		return NULL;
	}

	const uint8_t byte = data[*pos];

	const bool is_hufffman = (byte & 0x80) != 0;

	const HpackVariableIntegerResult length_res = decode_hpack_variable_integer(pos, size, data, 7);

	if(length_res.is_error) {
		return NULL;
	}

	const HpackVariableInteger length = length_res.data.value;

	if(*pos >= size) {
		return NULL;
	}

	// it is okay if (*pos) + length == size here, that means the string literal goes until the end!
	if(((*pos) + length) > size) {
		return NULL;
	}

	const SizedBuffer raw_bytes = {
		.data = (void*)(data + (*pos)),
		.size = length,
	};

	(*pos) += length;

	if(is_hufffman) {
		const HuffmanResult huffman_res = decode_bytes_huffman_with_global_data_setup(raw_bytes);

		if(huffman_res.is_error) {
			return NULL;
		}
		// TODO: huffman can technically produce 0 bytes, but then the encoder did some bad thing,

#ifndef NDEBUG
		assert(strlen(huffman_res.data.result.data) == huffman_res.data.result.size);
#endif

		// so we are just ignoring that
		return (char*)huffman_res.data.result.data;
	}

	char* value = malloc(length + 1);

	if(value == NULL) {
		return NULL;
	}

	value[length] = 0;
	memcpy(value, raw_bytes.data, raw_bytes.size);

	return value;
}

NODISCARD static size_t get_dynamic_entry_size(const HpackHeaderDynamicEntry entry) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-4.1

	// The size of an entry is the sum of its name's length in octets its value's length in octets,
	// and 32.

	return strlen(entry.key) + strlen(entry.value) + 32;
}

static void insert_entry_into_dynamic_table(HpackState* const state,
                                            const HpackHeaderDynamicEntry new_entry) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-4.4

	const size_t new_size = get_dynamic_entry_size(new_entry);

	if(new_size > state->max_dynamic_table_byte_size) {

		// free the entire table, this is not per se an error
		hpack_dynamic_table_free(&(state->dynamic_table));

		state->current_dynamic_table_byte_size = 0;

		return;
	}

	// clean until we have space
	while(state->current_dynamic_table_byte_size + new_size > state->max_dynamic_table_byte_size) {

		HpackHeaderDynamicEntry* entry = hpack_dynamic_table_pop_at_end(&(state->dynamic_table));

		if(entry == NULL) {
			// should not occur, if new_size doesn't fit at all, we should be able to free the
			// dynamic table with the if condition at the start
			break;
		}

		const size_t entry_size = get_dynamic_entry_size(*entry);
		free_dynamic_entry(*entry);
		state->current_dynamic_table_byte_size -= entry_size;
	}

	// finally insert the entry

	const HpackHeaderDynamicEntry new_entry_dup = { .key = strdup(new_entry.key),
		                                            .value =
		                                                strdup_null_agnostic(new_entry.value) };

	bool success = hpack_dynamic_table_insert_at_start(&(state->dynamic_table), new_entry_dup);

	if(!success) {
		return;
	}

	state->current_dynamic_table_byte_size += new_size;
}

NODISCARD static int parse_hpack_literal_header_field_with_incremental_indexing(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers,
    HpackState* const state) {
	// Literal Header Field with Incremental Indexing:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 1 |      Index (6+)       |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	// or

	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 1 |           0           |
	// +---+---+-----------------------+
	// | H |     Name Length (7+)      |
	// +---+---------------------------+
	// |  Name String (Length octets)  |
	// +---+---------------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const HpackVariableIntegerResult index_res = decode_hpack_variable_integer(pos, size, data, 6);

	if(index_res.is_error) {
		return -1;
	}

	const HpackVariableInteger index = index_res.data.value;

	// the name is the value from a table or a literal value
	char* header_key = NULL;

	if(index == 0) {
		// second variant

		char* string_literal_result = parse_literal_string_value(pos, size, data);

		if(string_literal_result == NULL) {
			return -5;
		}

		header_key = string_literal_result;

	} else {
		// first variant

		const HpackHeaderEntryResult entry = hpack_get_table_entry_at(state, index);

		if(entry.is_error) {
			return -3;
		}

		header_key = entry.value.key;
		free(entry.value.value);
	}

	if(*pos >= size) {
		return -8;
	}

	char* header_value = parse_literal_string_value(pos, size, data);

	if(header_value == NULL) {
		return -6;
	}

	const HttpHeaderField header_field = {
		.key = tstr_own_cstr(header_key),
		.value = tstr_own_cstr(header_value),
	};

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) {
		free_http_header_field(header_field);
		return -3;
	}

	const HpackHeaderDynamicEntry entry = {
		.key = header_key,
		.value = header_value,
	};

	insert_entry_into_dynamic_table(state, entry);

	return 0;
}

NODISCARD static int parse_hpack_dynamic_table_size_update(size_t* pos, const size_t size,
                                                           const uint8_t* const data,
                                                           HpackState* const state) {
	// Dynamic Table Size Update:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 1 |   Max size (5+)   |
	// +---+---------------------------+

	const HpackVariableIntegerResult new_size = decode_hpack_variable_integer(pos, size, data, 5);

	if(new_size.is_error) {
		return -1;
	}

	// TODO(Totto): this can't be greater than the http2 setting, but this is mostly used for
	// setting it to 0, to evict things, so not a priority to check that right now
	set_hpack_state_setting(state, new_size.data.value);

	return 0;
}

NODISCARD static int parse_hpack_literal_header_field_never_indexed(size_t* pos, const size_t size,
                                                                    const uint8_t* const data,
                                                                    HttpHeaderFields* const headers,
                                                                    const HpackState* const state) {
	// Literal Header Field Never Indexed:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 1 |  Index (4+)   |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	// or

	//    0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 1 |       0       |
	// +---+---+-----------------------+
	// | H |     Name Length (7+)      |
	// +---+---------------------------+
	// |  Name String (Length octets)  |
	// +---+---------------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const HpackVariableIntegerResult index_res = decode_hpack_variable_integer(pos, size, data, 4);

	if(index_res.is_error) {
		return -1;
	}

	const HpackVariableInteger index = index_res.data.value;

	// the name is the value from a table or a literal value
	char* header_key = NULL;

	if(index == 0) {
		// second variant

		char* string_literal_result = parse_literal_string_value(pos, size, data);

		if(string_literal_result == NULL) {
			return -5;
		}

		header_key = string_literal_result;

	} else {
		// first variant

		const HpackHeaderEntryResult entry = hpack_get_table_entry_at(state, index);

		if(entry.is_error) {
			return -3;
		}

		header_key = entry.value.key;
		free(entry.value.value);
	}

	if(*pos >= size) {
		return -8;
	}

	char* header_value = parse_literal_string_value(pos, size, data);

	if(header_value == NULL) {
		return -6;
	}

	const HttpHeaderField header_field = {
		.key = tstr_own_cstr(header_key),
		.value = tstr_own_cstr(header_value),
	};

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) {
		free_http_header_field(header_field);
		return -3;
	}

	return 0;
}

NODISCARD static int parse_hpack_literal_header_field_without_indexing(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers,
    const HpackState* const state) {
	// Literal Header Field without Indexing:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.2
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 0 |  Index (4+)   |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	// or

	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 0 |       0       |
	// +---+---+-----------------------+
	// | H |     Name Length (7+)      |
	// +---+---------------------------+
	// |  Name String (Length octets)  |
	// +---+---------------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const HpackVariableIntegerResult index_res = decode_hpack_variable_integer(pos, size, data, 4);

	if(index_res.is_error) {
		return -1;
	}

	const HpackVariableInteger index = index_res.data.value;

	// the name is the value from a table or a literal value
	char* header_key = NULL;

	if(index == 0) {
		// second variant

		char* string_literal_result = parse_literal_string_value(pos, size, data);

		if(string_literal_result == NULL) {
			return -5;
		}

		header_key = string_literal_result;

	} else {
		// first variant

		const HpackHeaderEntryResult entry = hpack_get_table_entry_at(state, index);

		if(entry.is_error) {
			return -3;
		}

		header_key = entry.value.key;
		free(entry.value.value);
	}

	if(*pos >= size) {
		return -8;
	}

	char* header_value = parse_literal_string_value(pos, size, data);

	if(header_value == NULL) {
		return -6;
	}

	const HttpHeaderField header_field = {
		.key = tstr_own_cstr(header_key),
		.value = tstr_own_cstr(header_value),
	};

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) {
		free_http_header_field(header_field);
		return -3;
	}

	return 0;
}

NODISCARD static Http2HpackDecompressResult
http2_hpack_decompress_data_impl(HpackState* const state, const SizedBuffer input) {

	size_t pos = 0;
	const size_t size = input.size;

	const uint8_t* const data = (uint8_t*)input.data;

	HttpHeaderFields result = TVEC_EMPTY(HttpHeaderField);
	const char* error = "None";

	while(pos < size) {
		uint8_t byte = data[pos];

		if((byte & 0x80) != 0) {
			// Indexed Header Field:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
			//    0   1   2   3   4   5   6   7
			//  +---+---+---+---+---+---+---+---+
			//  | 1 |        Index (7+)         |
			//  +---+---------------------------+
			const int res = parse_hpack_indexed_header_field(&pos, size, data, &result, state);
			if(res < 0) {
				error = "error in parsing indexed header field";
				goto return_error;
			}
		} else if((byte & 0xC0) == 0x40) {
			// Literal Header Field with Incremental Indexing:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 1 |      Index (6+)       |
			// +---+---+-----------------------+
			// ...
			const int res = parse_hpack_literal_header_field_with_incremental_indexing(
			    &pos, size, data, &result, state);
			if(res < 0) {
				error = "error in parsing literal header field with incremental indexing";
				goto return_error;
			}
		} else if((byte & 0xE0) == 0x20) {
			// Dynamic Table Size Update:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 1 |   Max size (5+)   |
			// +---+---------------------------+
			const int res = parse_hpack_dynamic_table_size_update(&pos, size, data, state);
			if(res < 0) {
				error = "error in parsing dynamic table size update";
				goto return_error;
			}
		} else if((byte & 0xF0) == 0x10) {
			// Literal Header Field Never Indexed:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 0 | 1 |  Index (4+)   |
			// +---+---+-----------------------+
			// ...
			const int res =
			    parse_hpack_literal_header_field_never_indexed(&pos, size, data, &result, state);
			if(res < 0) {
				error = "error in parsing literal header field never indexed";
				goto return_error;
			}
		} else {
			assert(
			    (byte & 0xF0) == 0 &&
			    "this should always be true logically, this is an implementation error otherwise");

			// Literal Header Field without Indexing:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.2
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 0 | 0 |  Index (4+)   |
			// +---+---+-----------------------+
			// ...
			const int res =
			    parse_hpack_literal_header_field_without_indexing(&pos, size, data, &result, state);
			if(res < 0) {
				error = "error in parsing literal header field without indexing";
				goto return_error;
			}
		}
	}

	return (Http2HpackDecompressResult){ .is_error = false,
		                                 .data = {
		                                     .result = result,
		                                 } };

return_error:

	free_http_header_fields(&result);
	return (Http2HpackDecompressResult){ .is_error = true,
		                                 .data = {
		                                     .error = error,
		                                 } };
}

NODISCARD HpackState* get_default_hpack_state(size_t max_dynamic_table_byte_size) {

	HpackState* state = malloc(sizeof(HpackState));

	if(state == NULL) {
		return NULL;
	}

	*state = (HpackState){
		.dynamic_table = hpack_dynamic_table_empty(),
		.current_dynamic_table_byte_size = 0,
		.max_dynamic_table_byte_size = max_dynamic_table_byte_size,
	};

	return state;
}

static void dynamic_table_evict_entries_on_table_change(HpackState* const state) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-4.3

	if(state->max_dynamic_table_byte_size == 0) {

		// free the entire table
		hpack_dynamic_table_free(&(state->dynamic_table));

		state->current_dynamic_table_byte_size = 0;

		return;
	}

	while(state->current_dynamic_table_byte_size > state->max_dynamic_table_byte_size) {

		HpackHeaderDynamicEntry* entry = hpack_dynamic_table_pop_at_end(&(state->dynamic_table));

		if(entry == NULL) {
			// can occur if the max_dynamic_table_byte_size is smaller tan the size of the first
			// entry (last to be popped)
			break;
		}

		const size_t entry_size = get_dynamic_entry_size(*entry);
		free_dynamic_entry(*entry);
		state->current_dynamic_table_byte_size -= entry_size;
	}
}

void set_hpack_state_setting(HpackState* const state, size_t max_dynamic_table_byte_size) {

	state->max_dynamic_table_byte_size = max_dynamic_table_byte_size;
	dynamic_table_evict_entries_on_table_change(state);
}

void free_hpack_state(HpackState* state) {

	hpack_dynamic_table_free(&(state->dynamic_table));
	free(state);
}

NODISCARD Http2HpackDecompressResult http2_hpack_decompress_data(HpackState* const state,
                                                                 const SizedBuffer input) {

	if(state == NULL) {
		return (Http2HpackDecompressResult){ .is_error = true,
			                                 .data = { .error = "state is NULL" } };
	}

	return http2_hpack_decompress_data_impl(state, input);
}

void global_initialize_http2_hpack_data(void) {
	global_initialize_http2_hpack_huffman_data();
	global_initialize_hpack_static_header_table_data();
}

void global_free_http2_hpack_data(void) {
	global_free_http2_hpack_huffman_data();
	global_free_hpack_static_header_table_data();
}

NODISCARD static SizedBuffer encode_single_header_field_dumb_literal(HttpHeaderField field) {

	// encode the value as:
	// Literal Header Field Never Indexed:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3

	// variant 2:

	//    0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 1 |       0       |
	// +---+---+-----------------------+
	// | H |     Name Length (7+)      |
	// +---+---------------------------+
	// |  Name String (Length octets)  |
	// +---+---------------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const size_t key_size = tstr_len(&field.key);
	const size_t value_size = tstr_len(&field.value);

	const size_t max_size = MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE +
	                        key_size + value_size + 1;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i++] = 0x10;

	{ // encode key / name

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, key_size, 7);

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&field.key), key_size);

		i += key_size;
	}

	{ // encode value

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, value_size, 7);

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&field.value), value_size);

		i += value_size;
	}

	if(i > buffer.size) {
		// NOTE: too much data used
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	void* new_data = realloc(buffer.data, i);

	if(new_data == NULL) {
		free_sized_buffer(buffer);
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	buffer.data = new_data;
	buffer.size = i;

	return buffer;
}

NODISCARD SizedBuffer http2_hpack_compress_data(HpackState* const state,
                                                const HttpHeaderFields header_fields) {

	// TODO: use more advanced methods of compression
	//  atm I only use "Literal Header Field Never Indexed", they take up some space, but are easy
	//  to create, as they don't require any lookup in the static or dynamic table and no huffman
	//  encoding

	// currently not using state alias the dynamic table
	UNUSED(state);

	SizedBuffer result = { .data = NULL, .size = 0 };

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, header_fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, header_fields, i);

		const SizedBuffer single_header_result = encode_single_header_field_dumb_literal(field);

		if(single_header_result.data == NULL) {
			free_sized_buffer(result);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t old_size = result.size;
		void* new_data = realloc(result.data, old_size + single_header_result.size);

		if(new_data == NULL) {
			free_sized_buffer(result);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		result.data = new_data;
		result.size += single_header_result.size;

		memcpy(((uint8_t*)result.data) + old_size, single_header_result.data,
		       single_header_result.size);
		free_sized_buffer(single_header_result);
	}

	return result;
}
