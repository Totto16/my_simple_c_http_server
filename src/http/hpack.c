#include "./hpack.h"
#include "./hpack_huffman.h"

#include "generated_hpack.h"

// see: https://datatracker.ietf.org/doc/html/rfc7541

typedef struct {
	char* key;
	char* value;
} HpackHeaderDynamicEntry;

TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(HpackHeaderDynamicEntry)

typedef TVEC_TYPENAME(HpackHeaderDynamicEntry) HpackHeaderDynamicEntries;

struct HpackStateImpl {
	HpackHeaderDynamicEntries dynamic_table;
	size_t max_dynamic_table_byte_size;
	size_t current_dynamic_table_byte_size;
};

typedef uint64_t HpackVariableInteger;

typedef struct {
	bool is_error;
	HpackVariableInteger value;
} HpackVariableIntegerResult;

static HpackVariableIntegerResult decode_hpack_variable_integer(size_t* pos, const size_t size,
                                                                const uint8_t* const data,
                                                                uint8_t prefix_bits) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-5.1

	uint8_t mask = (1 << prefix_bits) - 1;
	const uint8_t first_byte = (data[*pos]) & mask;

	if(first_byte < mask) {
		(*pos)++;
		return (HpackVariableIntegerResult){ .is_error = false,
			                                 .value = (HpackVariableInteger)first_byte };
	}

	(*pos)++;
	uint8_t amount = 0;

	HpackVariableInteger result = first_byte;

	while(true) {
		if((*pos) >= size) {
			// not more bytes available
			return (HpackVariableIntegerResult){ .is_error = true, .value = 0 };
		}

		const uint8_t byte = data[*pos];
		(*pos)++;

		if(amount >= sizeof(HpackVariableInteger) - 1) {
			// to many bytes, the index should not be larger than a uint64_t
			return (HpackVariableIntegerResult){ .is_error = true, .value = 0 };
		}

		result += (byte & 0x7F) << amount;
		if((byte & 0x80) == 0) {
			// this was the last byte
			break;
		}
		amount += 7;
	}

	return (HpackVariableIntegerResult){ .is_error = false, .value = result };
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
			                                 .value = strdup(static_entry.value) } };
	}

	const size_t dynamic_index = value - HPACK_STATIC_HEADER_TABLE_SIZE - 1;

	if(TVEC_LENGTH(HpackHeaderDynamicEntry, state->dynamic_table) <= dynamic_index) {
		return (HpackHeaderEntryResult){ .is_error = true };
	}

	const HpackHeaderDynamicEntry dynamic_entry =
	    TVEC_AT(HpackHeaderDynamicEntry, state->dynamic_table, dynamic_index);

	return (HpackHeaderEntryResult){ .is_error = false,
		                             .value = (HpackHeaderDynamicEntry){
		                                 .key = strdup(dynamic_entry.key),
		                                 .value = strdup(dynamic_entry.value) } };
}

static inline void free_dynamic_entry(const HpackHeaderDynamicEntry entry) {
	free(entry.key);
	free(entry.value);
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

	const HpackVariableIntegerResult index = decode_hpack_variable_integer(pos, size, data, 7);

	if(index.is_error) {
		return -1;
	}

	if(index.value == 0) {
		return -2;
	}

	const HpackHeaderEntryResult entry = hpack_get_table_entry_at(state, index.value);

	if(entry.is_error) {
		return -3;
	}

	const HttpHeaderField entry_value = { .key = entry.value.key, .value = entry.value.value };

	const auto insert_result = TVEC_PUSH(HttpHeaderField, headers, entry_value);

	if(insert_result != TvecResultOk) {
		free_dynamic_entry(entry.value);
		return -3;
	}

	return 0;
}

NODISCARD static int parse_hpack_literal_header_field_with_incremental_indexing(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers) {
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

	// TODO
	UNUSED(pos);
	UNUSED(size);
	UNUSED(data);
	UNUSED(headers);

	UNREACHABLE();
}

NODISCARD static int parse_hpack_dynamic_table_size_update(size_t* pos, const size_t size,
                                                           const uint8_t* const data,
                                                           HttpHeaderFields* const headers) {
	// Dynamic Table Size Update:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 1 |   Max size (5+)   |
	// +---+---------------------------+

	// TODO
	UNUSED(pos);
	UNUSED(size);
	UNUSED(data);
	UNUSED(headers);

	UNREACHABLE();
}

NODISCARD static int parse_hpack_literal_header_field_never_indexed(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers) {
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

	// TODO
	UNUSED(pos);
	UNUSED(size);
	UNUSED(data);
	UNUSED(headers);

	UNREACHABLE();
}

NODISCARD static int parse_hpack_literal_header_field_without_indexing(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers) {
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

	// TODO
	UNUSED(pos);
	UNUSED(size);
	UNUSED(data);
	UNUSED(headers);

	UNREACHABLE();
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
			    &pos, size, data, &result);
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
			const int res = parse_hpack_dynamic_table_size_update(&pos, size, data, &result);
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
			    parse_hpack_literal_header_field_never_indexed(&pos, size, data, &result);
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
			    parse_hpack_literal_header_field_without_indexing(&pos, size, data, &result);
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
		.dynamic_table = TVEC_EMPTY(HpackHeaderDynamicEntry),
		.current_dynamic_table_byte_size = 0,
		.max_dynamic_table_byte_size = max_dynamic_table_byte_size,
	};

	return state;
}

NODISCARD static size_t get_dynamic_entry_size(const HpackHeaderDynamicEntry entry) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-4.1

	// The size of an entry is the sum of its name's length in octets its value's length in octets,
	// and 32.

	return strlen(entry.key) + strlen(entry.value) + 32;
}

void set_hpack_state_setting(HpackState* const state, size_t max_dynamic_table_byte_size) {

	state->max_dynamic_table_byte_size = max_dynamic_table_byte_size;

	for(size_t i = TVEC_LENGTH(HpackHeaderDynamicEntry, state->dynamic_table);
	    (state->current_dynamic_table_byte_size > max_dynamic_table_byte_size) && (i != 0); --i) {

		HpackHeaderDynamicEntry entry =
		    TVEC_POP_GET(HpackHeaderDynamicEntry, &(state->dynamic_table));

		const size_t entry_size = get_dynamic_entry_size(entry);
		free_dynamic_entry(entry);
		state->current_dynamic_table_byte_size -= entry_size;
	}
}

void free_hpack_state(HpackState* state) {

	for(size_t i = 0; i < TVEC_LENGTH(HpackHeaderDynamicEntry, state->dynamic_table); ++i) {

		const HpackHeaderDynamicEntry entry =
		    TVEC_AT(HpackHeaderDynamicEntry, state->dynamic_table, i);

		free_dynamic_entry(entry);
	}

	TVEC_FREE(HpackHeaderDynamicEntry, &(state->dynamic_table));

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
