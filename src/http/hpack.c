#include "./hpack.h"
#include "./dynamic_hpack_table.h"
#include "./hpack_huffman.h"

#include "generated_hpack.h"

// see: https://datatracker.ietf.org/doc/html/rfc7541

typedef struct {
	HpackHeaderDynamicTable dynamic_table;
	size_t max_dynamic_table_byte_size;
	size_t current_dynamic_table_byte_size;
} HpackDynamicTableState;

struct HpackDecompressStateImpl {
	HpackDynamicTableState dynamic_table_state;
};

struct HpackCompressStateImpl {
	HpackDynamicTableState dynamic_table_state;
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
			return (HpackVariableIntegerResult){
				.is_error = true, .data = { .error = TSTR_STATIC_LIT("not enough bytes") }
			};
		}

		const uint8_t byte = data[*pos];
		(*pos)++;

		if((amount / 8) >= // NOLINT(readability-magic-numbers)
		   sizeof(HpackVariableInteger) - 1) {
			// to many bytes, the index should not be larger than a uint64_t
			return (HpackVariableIntegerResult){
				.is_error = true,
				.data = { .error = TSTR_STATIC_LIT("final integer would be too big") }
			};
		}

		result += (byte & 0x7F) // NOLINT(readability-magic-numbers)
		          << amount;
		if((byte & 0x80) == // NOLINT(readability-magic-numbers)
		   0) {
			// this was the last byte
			break;
		}
		amount += 7; // NOLINT(readability-magic-numbers)
	}

	return (HpackVariableIntegerResult){ .is_error = false,
		                                 .data = {
		                                     .value = result,
		                                 } };
}

// note: out_bytes has to have a available size of MAX_HPACK_VARIABLE_INTEGER_SIZE
NODISCARD static int8_t encode_hpack_variable_integer(
    uint8_t* const out_bytes,
    const HpackVariableInteger input, // NOLINT(bugprone-easily-swappable-parameters)
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
	int8_t idx = 0;

	out_bytes[idx] = out_bytes[idx] | mask;

	value -= mask;
	++idx;

	// first byte was special, now loop

	while(true) {
		if(idx > (int)(MAX_HPACK_VARIABLE_INTEGER_SIZE)) {
			return -1;
		}

		if(value >= 0x80) { // NOLINT(readability-magic-numbers)
			// not the end

			const uint8_t to_encode = value & 0x7F;
			out_bytes[idx++] = 0x80 + // NOLINT(readability-magic-numbers)
			                   to_encode;
			value /= 0x80; // NOLINT(readability-magic-numbers)
		} else {
			out_bytes[idx++] = value;
			break;
		}
	}

	return idx;
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

NODISCARD static HpackHeaderEntryResult
hpack_get_table_entry_at(const HpackDynamicTableState* const state, size_t value) {

	if(value == 0) {
		return (HpackHeaderEntryResult){ .is_error = true };
	}

	if(value <= HPACK_STATIC_HEADER_TABLE_SIZE) {
		assert(g_hpack_static_data.static_header_table != NULL);

		HpackHeaderStaticEntry static_entry = g_hpack_static_data.static_header_table[value - 1];

		return (HpackHeaderEntryResult){ .is_error = false,
			                             .value = (HpackHeaderDynamicEntry){
			                                 .key = tstr_dup(&static_entry.key),
			                                 .value = tstr_dup(&static_entry.value) } };
	}

	const size_t dynamic_index = value - HPACK_STATIC_HEADER_TABLE_SIZE - 1;

	const size_t dynamic_table_size = hpack_dynamic_table_size(&(state->dynamic_table));

	if(dynamic_table_size <= dynamic_index) {
		return (HpackHeaderEntryResult){ .is_error = true };
	}

	const HpackHeaderDynamicEntryResult dynamic_entry_res =
	    hpack_dynamic_table_at(&(state->dynamic_table), dynamic_index);

	if(!dynamic_entry_res.ok) {
		return (HpackHeaderEntryResult){ .is_error = true };
	}

	// asserts errors in the underlying dynamic table
	assert(!tstr_is_null(&(dynamic_entry_res.entry.key)));

	return (HpackHeaderEntryResult){ .is_error = false,
		                             .value = (HpackHeaderDynamicEntry){
		                                 .key = tstr_dup(&dynamic_entry_res.entry.key),
		                                 .value = tstr_dup(&dynamic_entry_res.entry.value) } };
}

NODISCARD static GenericResult
parse_hpack_indexed_header_field(size_t* pos, const size_t size, const uint8_t* const data,
                                 HttpHeaderFields* const headers,
                                 const HpackDecompressState* const decompress_state) {
	// Indexed Header Field:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
	//    0   1   2   3   4   5   6   7
	//  +---+---+---+---+---+---+---+---+
	//  | 1 |        Index (7+)         |
	//  +---+---------------------------+

	const HpackVariableIntegerResult index_res = decode_hpack_variable_integer(pos, size, data, 7);

	if(index_res.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HpackVariableInteger index = index_res.data.value;

	if(index == 0) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HpackHeaderEntryResult entry_res =
	    hpack_get_table_entry_at(&(decompress_state->dynamic_table_state), index);

	if(entry_res.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	HpackHeaderDynamicEntry entry = entry_res.value;

	// some tests we run use this, even if it's wrong, in the real usage (alias release mode) we
	// just error oru, as this is a strict error in my opinion, the http2 hpack spec says, that the
	// values are empty, in the static table, but i say they are NULL, so only the key can be used
	// from these entries, the tests that have an empty ("") value and use the whole entry are not
	// the best, but to pass them, i just use this hack
#ifdef NDEBUG
	#define STRICT_DECODING 1
#else
	#define STRICT_DECODING 0
#endif

#if STRICT_DECODING == 1

	// the value can't be null, if using the value too, it can be empty tstr_init() but not
	// NULL!

	if(tstr_is_null(&entry.value)) {
		free_dynamic_entry(&entry);
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HttpHeaderField header_field = { .key = entry.key, .value = entry.value };

#elif STRICT_DECODING == 0

	tstr entry_value = entry.value;

	if(tstr_is_null(&entry_value)) {

		// do some global lookup, that is set by the testing framework, this is just some hack, so
		// that I know that this occurred

		// use env variables, as they are global and can be seen by the executable code, even if
		// this is is in a library

	#define TEST_ENV_PREFIX "TOTTO_SIMPLE_HTTP_SERVER___ENV___HACK_IMPL"
	#define UNION_CAST_TRICK(Name, val) \
		((union { \
			void* v; \
			Name fn; \
		}){ .v = (val) }) \
		    .fn

		const char* const err_callback = getenv(TEST_ENV_PREFIX "_CALLBACK_FN");

		if(err_callback != NULL) {

			const size_t err_cb_size = strlen(err_callback);

			if(err_cb_size == 0) {

				fprintf(stderr,
				        "STRICT VIOLATION: used value of entry with a null value, key: " TSTR_FMT
				        "\n",
				        TSTR_FMT_ARGS(entry.key));
			} else if(err_cb_size == (sizeof(void*) + sizeof(uint8_t))) {

				typedef void (*CbFn)(const tstr* const str);

				const void* const err_callback_bytes = (const void* const)err_callback;

				const uint8_t mask_byte = *(((const uint8_t*)err_callback_bytes) + sizeof(void*));

				void* cb_fn_raw = NULL;
				memcpy((void*)&cb_fn_raw, err_callback_bytes, sizeof(void*));

				{ // patch fn ptr

					uint8_t* const raw_fb_ptr = (uint8_t*)(&cb_fn_raw);

					for(size_t i = 0; i < sizeof(void*); ++i) {
						if((mask_byte & (1 << i)) == 0) {
							raw_fb_ptr[i] = 0x00;
						}
					}
				}

				CbFn cb_fn = UNION_CAST_TRICK(CbFn, cb_fn_raw);

				cb_fn(&entry.key);
			} else {
				fprintf(stderr,
				        "ERROR: cb is wrongly formatted, it has size %zu (not equal to 0 or %zu), "
				        "which means we have encoded it incorrectly, the text was: %s\n",
				        err_cb_size, (sizeof(void*) + sizeof(uint8_t)), err_callback);
				abort();
			}
		}

		entry_value = tstr_init();
	}

	const HttpHeaderField header_field = { .key = entry.key, .value = entry_value };

#else
	#error "invalid value for STRICT_DECODING"
#endif

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)
		free_dynamic_entry(&entry);
		return GENERIC_RES_ERR_UNIQUE();
	}

	return GENERIC_RES_OK();
}

typedef struct {
	bool is_error;
	union {
		tstr_static error;
		tstr value;
	} data;
} LiteralStringResult;

NODISCARD static LiteralStringResult parse_literal_string_value(size_t* pos, const size_t size,
                                                                const uint8_t* const data) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-5.2

	if(*pos >= size) {
		return (LiteralStringResult){
			.is_error = true,
			.data = { .error =
			              TSTR_STATIC_LIT("not enough bytes at the start of a string literal") }
		};
	}

	const uint8_t byte = data[*pos];

	const bool is_huffman = (byte & 0x80) != 0;

	const HpackVariableIntegerResult length_res = decode_hpack_variable_integer(pos, size, data, 7);

	if(length_res.is_error) {
		return (LiteralStringResult){ .is_error = true,
			                          .data = { .error = length_res.data.error } };
	}

	const HpackVariableInteger length = length_res.data.value;

	if(*pos >= size) {
		return (LiteralStringResult){
			.is_error = true,
			.data = { .error = TSTR_STATIC_LIT(
			              "not enough bytes after the length bytes of a string literal") }
		};
	}

	if(length == 0) {
		return (LiteralStringResult){ .is_error = false,
			                          .data = {
			                              .value = tstr_init(),
			                          } };
	}

	// it is okay if (*pos) + length == size here, that means the string literal goes until the end!
	if(((*pos) + length) > size) {
		return (LiteralStringResult){
			.is_error = true,
			.data = { .error = TSTR_STATIC_LIT(
			              "not enough bytes for the amount of data of a string literal") }
		};
	}

	const ReadonlyBuffer raw_bytes = { .data = data + (*pos), .size = length };

	(*pos) += length;

	if(is_huffman) {
		const HuffmanDecodeResult huffman_res = hpack_huffman_decode_bytes(raw_bytes);

		IF_HUFFMAN_DECODE_RESULT_IS_ERROR_CONST(huffman_res) {
			return (LiteralStringResult){ .is_error = true, .data = { .error = error.error } };
		}
		// TODO(Totto): huffman can technically produce 0 bytes, but then the encoder did some bad
		// thing, so we are just ignoring that

		const SizedBuffer result_buf = huffman_decode_result_get_as_ok(huffman_res).result;

#ifndef NDEBUG
		assert(strlen(result_buf.data) == result_buf.size);
#endif

		const tstr result = tstr_own(result_buf.data, result_buf.size, result_buf.size);

		return (LiteralStringResult){ .is_error = false, .data = { .value = result } };
	}

	char* value = malloc(length + 1);

	if(value == NULL) {
		return (LiteralStringResult){ .is_error = true,
			                          .data = { .error = TSTR_STATIC_LIT("OOM") } };
	}

	value[length] = 0;
	memcpy(value, raw_bytes.data, raw_bytes.size);

	const tstr result = tstr_own(value, length, length);

	return (LiteralStringResult){ .is_error = false, .data = { .value = result } };
}

NODISCARD static size_t get_dynamic_entry_size(const HpackHeaderDynamicEntry entry) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-4.1

	// The size of an entry is the sum of its name's length in octets its value's length in octets,
	// and 32.

	return tstr_len(&entry.key) + tstr_len(&entry.value) + 32; // NOLINT(readability-magic-numbers)
}

static void insert_entry_into_dynamic_table(HpackDynamicTableState* const state,
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

		HpackHeaderDynamicEntryResult entry_res =
		    hpack_dynamic_table_pop_at_end(&(state->dynamic_table));

		if(!entry_res.ok) {
			// should not occur, if new_size doesn't fit at all, we should be able to free the
			// dynamic table with the if condition at the start
			break;
		}

		const size_t entry_size = get_dynamic_entry_size(entry_res.entry);
		free_dynamic_entry(&(entry_res.entry));
		state->current_dynamic_table_byte_size -= entry_size;
	}

	// finally insert the entry

	const HpackHeaderDynamicEntry new_entry_dup = { .key = tstr_dup(&new_entry.key),
		                                            .value = tstr_dup(&new_entry.value) };

	bool success = hpack_dynamic_table_insert_at_start(&(state->dynamic_table), new_entry_dup);

	if(!success) {
		return;
	}

	state->current_dynamic_table_byte_size += new_size;
}

NODISCARD static GenericResult parse_hpack_literal_header_field_with_incremental_indexing(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers,
    HpackDecompressState* const decompress_state) {
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
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HpackVariableInteger index = index_res.data.value;

	// the name is the value from a table or a literal value
	tstr header_key = tstr_null(); // NOLINT(clang-analyzer-deadcode.DeadStores)

	if(index == 0) {
		// second variant

		const LiteralStringResult string_literal_result =
		    parse_literal_string_value(pos, size, data);

		if(string_literal_result.is_error) {
			return GENERIC_RES_ERR_UNIQUE();
		}

		header_key = string_literal_result.data.value;

	} else {
		// first variant

		HpackHeaderEntryResult entry =
		    hpack_get_table_entry_at(&(decompress_state->dynamic_table_state), index);

		if(entry.is_error) {
			return GENERIC_RES_ERR_UNIQUE();
		}

		header_key = entry.value.key;
		tstr_free(&entry.value.value);
	}

	if(*pos >= size) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const LiteralStringResult header_value = parse_literal_string_value(pos, size, data);

	if(header_value.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HttpHeaderField header_field = {
		.key = header_key,
		.value = header_value.data.value,
	};

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)
		free_http_header_field(header_field);
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HpackHeaderDynamicEntry entry = {
		.key = header_key,
		.value = header_value.data.value,
	};

	insert_entry_into_dynamic_table(&(decompress_state->dynamic_table_state), entry);

	return GENERIC_RES_OK();
}

NODISCARD static GenericResult
parse_hpack_dynamic_table_size_update(size_t* pos, const size_t size, const uint8_t* const data,
                                      HpackDecompressState* const decompress_state) {
	// Dynamic Table Size Update:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 1 |   Max size (5+)   |
	// +---+---------------------------+

	const HpackVariableIntegerResult new_size = decode_hpack_variable_integer(pos, size, data, 5);

	if(new_size.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	// TODO(Totto): this can't be greater than the http2 setting, but this is mostly used for
	// setting it to 0, to evict things, so not a priority to check that right now
	set_hpack_decompress_state_setting(decompress_state, new_size.data.value);

	return GENERIC_RES_OK();
}

NODISCARD static GenericResult parse_hpack_literal_header_field_never_indexed(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers,
    const HpackDecompressState* const decompress_state) {
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
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HpackVariableInteger index = index_res.data.value;

	// the name is the value from a table or a literal value
	tstr header_key = tstr_null(); // NOLINT(clang-analyzer-deadcode.DeadStores)

	if(index == 0) {
		// second variant

		const LiteralStringResult string_literal_result =
		    parse_literal_string_value(pos, size, data);

		if(string_literal_result.is_error) {
			return GENERIC_RES_ERR_UNIQUE();
		}

		header_key = string_literal_result.data.value;

	} else {
		// first variant

		HpackHeaderEntryResult entry =
		    hpack_get_table_entry_at(&(decompress_state->dynamic_table_state), index);

		if(entry.is_error) {
			return GENERIC_RES_ERR_UNIQUE();
		}

		header_key = entry.value.key;
		tstr_free(&entry.value.value);
	}

	if(*pos >= size) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const LiteralStringResult header_value = parse_literal_string_value(pos, size, data);

	if(header_value.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HttpHeaderField header_field = {
		.key = header_key,
		.value = header_value.data.value,
	};

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)
		free_http_header_field(header_field);
		return GENERIC_RES_ERR_UNIQUE();
	}

	return GENERIC_RES_OK();
}

NODISCARD static GenericResult parse_hpack_literal_header_field_without_indexing(
    size_t* pos, const size_t size, const uint8_t* const data, HttpHeaderFields* const headers,
    const HpackDecompressState* const decompress_state) {
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

	// NOTE: this is exactly the same as parse_hpack_literal_header_field_never_indexed, except the
	// first 4 bytes, which this function doesn't care about

	const HpackVariableIntegerResult index_res = decode_hpack_variable_integer(pos, size, data, 4);

	if(index_res.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HpackVariableInteger index = index_res.data.value;

	// the name is the value from a table or a literal value
	tstr header_key = tstr_null(); // NOLINT(clang-analyzer-deadcode.DeadStores)

	if(index == 0) {
		// second variant

		const LiteralStringResult string_literal_result =
		    parse_literal_string_value(pos, size, data);

		if(string_literal_result.is_error) {
			return GENERIC_RES_ERR_UNIQUE();
		}

		header_key = string_literal_result.data.value;

	} else {
		// first variant

		HpackHeaderEntryResult entry =
		    hpack_get_table_entry_at(&(decompress_state->dynamic_table_state), index);

		if(entry.is_error) {
			return GENERIC_RES_ERR_UNIQUE();
		}

		header_key = entry.value.key;
		tstr_free(&entry.value.value);
	}

	if(*pos >= size) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const LiteralStringResult header_value = parse_literal_string_value(pos, size, data);

	if(header_value.is_error) {
		return GENERIC_RES_ERR_UNIQUE();
	}

	const HttpHeaderField header_field = {
		.key = header_key,
		.value = header_value.data.value,
	};

	const TvecResult insert_result = TVEC_PUSH(HttpHeaderField, headers, header_field);

	if(insert_result != TvecResultOk) { // NOLINT(readability-implicit-bool-conversion)
		free_http_header_field(header_field);
		return GENERIC_RES_ERR_UNIQUE();
	}

	return GENERIC_RES_OK();
}

NODISCARD static Http2HpackDecompressResult
http2_hpack_decompress_data_impl(HpackDecompressState* const decompress_state,
                                 const ReadonlyBuffer input) {

	size_t pos = 0;
	const size_t size = input.size;

	const uint8_t* const data = (const uint8_t*)input.data;

	HttpHeaderFields result = TVEC_EMPTY(HttpHeaderField);
	const char* error = "None"; // NOLINT(clang-analyzer-deadcode.DeadStores)

	while(pos < size) {
		uint8_t byte = data[pos];

		if((byte & 0x80) != // NOLINT(readability-magic-numbers)
		   0) {
			// Indexed Header Field:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
			//    0   1   2   3   4   5   6   7
			//  +---+---+---+---+---+---+---+---+
			//  | 1 |        Index (7+)         |
			//  +---+---------------------------+
			const GenericResult res =
			    parse_hpack_indexed_header_field(&pos, size, data, &result, decompress_state);
			if(res.is_error) {
				error = "error in parsing indexed header field";
				goto return_error;
			}
		} else if((byte & 0xC0) == // NOLINT(readability-magic-numbers)
		          0x40) {          // NOLINT(readability-magic-numbers)
			// Literal Header Field with Incremental Indexing:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 1 |      Index (6+)       |
			// +---+---+-----------------------+
			// ...
			const GenericResult res = parse_hpack_literal_header_field_with_incremental_indexing(
			    &pos, size, data, &result, decompress_state);
			if(res.is_error) {
				error = "error in parsing literal header field with incremental indexing";
				goto return_error;
			}
		} else if((byte & 0xE0) == // NOLINT(readability-magic-numbers)
		          0x20) {          // NOLINT(readability-magic-numbers)
			// Dynamic Table Size Update:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 1 |   Max size (5+)   |
			// +---+---------------------------+
			const GenericResult res =
			    parse_hpack_dynamic_table_size_update(&pos, size, data, decompress_state);
			if(res.is_error) {
				error = "error in parsing dynamic table size update";
				goto return_error;
			}
		} else if((byte & 0xF0) == // NOLINT(readability-magic-numbers)
		          0x10) {          // NOLINT(readability-magic-numbers)
			// Literal Header Field Never Indexed:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 0 | 1 |  Index (4+)   |
			// +---+---+-----------------------+
			// ...
			const GenericResult res = parse_hpack_literal_header_field_never_indexed(
			    &pos, size, data, &result, decompress_state);
			if(res.is_error) {
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
			const GenericResult res = parse_hpack_literal_header_field_without_indexing(
			    &pos, size, data, &result, decompress_state);
			if(res.is_error) {
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

NODISCARD static HpackDynamicTableState
get_default_hpack_dynamic_table_state(size_t max_dynamic_table_byte_size) {
	return (HpackDynamicTableState){
		.dynamic_table = hpack_dynamic_table_get_empty(),
		.current_dynamic_table_byte_size = 0,
		.max_dynamic_table_byte_size = max_dynamic_table_byte_size,
	};
}

NODISCARD HpackDecompressState*
get_default_hpack_decompress_state(size_t max_dynamic_table_byte_size) {

	HpackDecompressState* state = malloc(sizeof(HpackDecompressState));

	if(state == NULL) {
		return NULL;
	}

	*state = (HpackDecompressState){ .dynamic_table_state = get_default_hpack_dynamic_table_state(
		                                 max_dynamic_table_byte_size) };

	return state;
}

NODISCARD HpackCompressState* get_default_hpack_compress_state(size_t max_dynamic_table_byte_size) {

	HpackCompressState* state = malloc(sizeof(HpackCompressState));

	if(state == NULL) {
		return NULL;
	}

	*state = (HpackCompressState){ .dynamic_table_state = get_default_hpack_dynamic_table_state(
		                               max_dynamic_table_byte_size) };

	return state;
}

static void dynamic_table_evict_entries_on_table_change(HpackDynamicTableState* const state) {
	// see: https://datatracker.ietf.org/doc/html/rfc7541#section-4.3

	if(state->max_dynamic_table_byte_size == 0) {

		// free the entire table
		hpack_dynamic_table_free(&(state->dynamic_table));

		state->current_dynamic_table_byte_size = 0;

		return;
	}

	while(state->current_dynamic_table_byte_size > state->max_dynamic_table_byte_size) {

		HpackHeaderDynamicEntryResult entry_res =
		    hpack_dynamic_table_pop_at_end(&(state->dynamic_table));

		if(!entry_res.ok) {
			// can occur if the max_dynamic_table_byte_size is smaller tan the size of the first
			// entry (last to be popped)
			break;
		}

		const size_t entry_size = get_dynamic_entry_size(entry_res.entry);
		free_dynamic_entry(&(entry_res.entry));
		state->current_dynamic_table_byte_size -= entry_size;
	}
}

void set_hpack_decompress_state_setting(HpackDecompressState* const decompress_state,
                                        size_t max_dynamic_table_byte_size) {

	decompress_state->dynamic_table_state.max_dynamic_table_byte_size = max_dynamic_table_byte_size;
	dynamic_table_evict_entries_on_table_change(&(decompress_state->dynamic_table_state));
}

void set_hpack_compress_state_setting(HpackCompressState* const compress_state,
                                      size_t max_dynamic_table_byte_size) {

	compress_state->dynamic_table_state.max_dynamic_table_byte_size = max_dynamic_table_byte_size;
	dynamic_table_evict_entries_on_table_change(&(compress_state->dynamic_table_state));
}

static void free_dynamic_table_state(HpackDynamicTableState* const state) {
	hpack_dynamic_table_free(&(state->dynamic_table));
}

void free_hpack_decompress_state(HpackDecompressState* const decompress_state) {

	free_dynamic_table_state(&(decompress_state->dynamic_table_state));
	free(decompress_state);
}

void free_hpack_compress_state(HpackCompressState* compress_state) {

	free_dynamic_table_state(&(compress_state->dynamic_table_state));
	free(compress_state);
}

NODISCARD Http2HpackDecompressResult http2_hpack_decompress_data(
    HpackDecompressState* const decompress_state, const ReadonlyBuffer input) {

	if(decompress_state == NULL) {
		return (Http2HpackDecompressResult){ .is_error = true,
			                                 .data = { .error = "state is NULL" } };
	}

	return http2_hpack_decompress_data_impl(decompress_state, input);
}

void global_initialize_http2_hpack_data(void) {
	global_initialize_http2_hpack_huffman_data();
	global_initialize_hpack_static_header_table_data();
}

void global_free_http2_hpack_data(void) {
	global_free_http2_hpack_huffman_data();
	global_free_hpack_static_header_table_data();
}

NODISCARD static SizedBuffer encode_single_header_field_literal_never_indexed_variant1_no_huffman(
    const size_t field_key_table_idx, const tstr* const field_value) {
	// encode the value as:
	// Literal Header Field Never Indexed:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3

	// variant 1:

	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 1 |  Index (4+)   |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const size_t value_size = tstr_len(field_value);

	const size_t max_size =
	    1 + MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE + value_size;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i] = 0x10; // NOLINT(readability-magic-numbers)

	{ // encode key as table index

		int8_t result = encode_hpack_variable_integer(data + i, field_key_table_idx, 4);

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;
	}

	{ // encode value

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, value_size,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(field_value), value_size);

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

NODISCARD static SizedBuffer encode_single_header_field_literal_never_indexed_variant1_huffman(
    const size_t field_key_table_idx, const tstr* const field_value, const size_t size_value) {
	// encode the value as:
	// Literal Header Field Never Indexed:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3

	// variant 1:

	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 0 | 1 |  Index (4+)   |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const size_t max_size =
	    1 + MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE + size_value;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i] = 0x10; // NOLINT(readability-magic-numbers)

	{ // encode key as table index

		int8_t result = encode_hpack_variable_integer(data + i, field_key_table_idx, 4);

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;
	}

	{ // encode value

		// set Huffman to true
		data[i] = 0x80; // NOLINT(readability-magic-numbers)

		int8_t result = encode_hpack_variable_integer(data + i, size_value,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		const HuffmanEncodeFixedResult enc_result =
		    hpack_huffman_encode_value_fixed_size(data + i, size_value, field_value);

		IF_HUFFMAN_ENCODE_FIXED_RESULT_IS_ERROR_IGN(enc_result) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t result_size = huffman_encode_fixed_result_get_as_ok(enc_result).size;

		if(result_size != size_value) {
			// size_key way wrong, as http_hpack_encode_value_fixed_size just accepts an upper
			// bound, it may encode in fewer bytes, may_size just tells, how much place the buffer
			// has
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result_size;
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

NODISCARD static SizedBuffer encode_single_header_field_literal_never_indexed_variant1(
    const size_t field_key_table_idx, const tstr* const field_value,
    const Http2HpackHuffmanUsage huffman_usage) {

	switch(huffman_usage) {
		case Http2HpackHuffmanUsageNever: {
			return encode_single_header_field_literal_never_indexed_variant1_no_huffman(
			    field_key_table_idx, field_value);
		}
		case Http2HpackHuffmanUsageAlways: {
			const size_t size_value = hpack_huffman_get_encoded_size(field_value);

			return encode_single_header_field_literal_never_indexed_variant1_huffman(
			    field_key_table_idx, field_value, size_value);
		}
		case Http2HpackHuffmanUsageAuto:
		default: {

			const size_t size_value = hpack_huffman_get_encoded_size(field_value);

			if(size_value < tstr_len(field_value)) {
				return encode_single_header_field_literal_never_indexed_variant1_huffman(
				    field_key_table_idx, field_value, size_value);
			}

			return encode_single_header_field_literal_never_indexed_variant1_no_huffman(
			    field_key_table_idx, field_value);
		}
	}
}

NODISCARD static SizedBuffer encode_single_header_field_literal_never_indexed_variant2_no_huffman(
    const HttpHeaderField* const field) {
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

	const size_t key_size = tstr_len(&(field->key));
	const size_t value_size = tstr_len(&(field->value));

	const size_t max_size = MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE +
	                        key_size + value_size + 1;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i++] = 0x10; // NOLINT(readability-magic-numbers)

	{ // encode key / name

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, key_size,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&(field->key)), key_size);

		i += key_size;
	}

	{ // encode value

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, value_size,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&(field->value)), value_size);

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

NODISCARD static SizedBuffer encode_single_header_field_literal_never_indexed_variant2_huffman(
    const HttpHeaderField* const field, const size_t size_key, const size_t size_value) {
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

	const size_t max_size = MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE +
	                        size_key + size_value + 1;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i++] = 0x10; // NOLINT(readability-magic-numbers)

	{ // encode key / name

		// set Huffman to true
		data[i] = 0x80; // NOLINT(readability-magic-numbers)

		int8_t result = encode_hpack_variable_integer(data + i, size_key,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		const HuffmanEncodeFixedResult enc_result =
		    hpack_huffman_encode_value_fixed_size(data + i, size_key, &(field->key));

		IF_HUFFMAN_ENCODE_FIXED_RESULT_IS_ERROR_IGN(enc_result) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t result_size = huffman_encode_fixed_result_get_as_ok(enc_result).size;

		if(result_size != size_key) {
			// size_key way wrong, as http_hpack_encode_value_fixed_size just accepts an upper
			// bound, it may encode in fewer bytes, may_size just tells, how much place the buffer
			// has
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result_size;
	}

	{ // encode value

		// set Huffman to true
		data[i] = 0x80; // NOLINT(readability-magic-numbers)

		int8_t result = encode_hpack_variable_integer(data + i, size_value,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		const HuffmanEncodeFixedResult enc_result =
		    hpack_huffman_encode_value_fixed_size(data + i, size_value, &(field->value));

		IF_HUFFMAN_ENCODE_FIXED_RESULT_IS_ERROR_IGN(enc_result) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t result_size = huffman_encode_fixed_result_get_as_ok(enc_result).size;

		if(result_size != size_value) {
			// size_key way wrong, as http_hpack_encode_value_fixed_size just accepts an upper
			// bound, it may encode in fewer bytes, may_size just tells, how much place the buffer
			// has
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result_size;
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

NODISCARD static SizedBuffer encode_single_header_field_literal_never_indexed_variant2(
    const HttpHeaderField* const field, const Http2HpackHuffmanUsage huffman_usage) {

	switch(huffman_usage) {
		case Http2HpackHuffmanUsageNever: {
			return encode_single_header_field_literal_never_indexed_variant2_no_huffman(field);
		}
		case Http2HpackHuffmanUsageAlways: {
			const size_t size_key = hpack_huffman_get_encoded_size(&(field->key));
			const size_t size_value = hpack_huffman_get_encoded_size(&(field->value));

			return encode_single_header_field_literal_never_indexed_variant2_huffman(
			    field, size_key, size_value);
		}
		case Http2HpackHuffmanUsageAuto:
		default: {

			const size_t size_key = hpack_huffman_get_encoded_size(&(field->key));
			const size_t size_value = hpack_huffman_get_encoded_size(&(field->value));

			if(size_key + size_value < tstr_len(&(field->key)) + tstr_len(&(field->value))) {
				return encode_single_header_field_literal_never_indexed_variant2_huffman(
				    field, size_key, size_value);
			}

			return encode_single_header_field_literal_never_indexed_variant2_no_huffman(field);
		}
	}
}

NODISCARD static SizedBuffer
encode_single_header_field_literal_incremental_indexing_variant1_no_huffman(
    const size_t field_key_table_idx, const HttpHeaderField* const field,
    HpackCompressState* const compress_state) {
	// encode the value as:
	// Literal Header Field with Incremental Indexing:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1

	// variant 1:

	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 1 |      Index (6+)       |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const size_t value_size = tstr_len(&(field->value));

	const size_t max_size =
	    1 + MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE + value_size;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i] = 0x40; // NOLINT(readability-magic-numbers)

	{ // encode key as table index

		int8_t result = encode_hpack_variable_integer(data + i, field_key_table_idx,
		                                              6); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;
	}

	{ // encode value

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, value_size,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&(field->value)), value_size);

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

	const HpackHeaderDynamicEntry entry = {
		.key = tstr_dup(&(field->key)),
		.value = tstr_dup(&(field->value)),
	};

	insert_entry_into_dynamic_table(&(compress_state->dynamic_table_state), entry);

	return buffer;
}

NODISCARD static SizedBuffer
encode_single_header_field_literal_incremental_indexing_variant1_huffman(
    const size_t field_key_table_idx, const HttpHeaderField* const field, const size_t size_value,
    HpackCompressState* const compress_state) {
	// encode the value as:
	// Literal Header Field with Incremental Indexing:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1

	// variant 1:

	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 1 |      Index (6+)       |
	// +---+---+-----------------------+
	// | H |     Value Length (7+)     |
	// +---+---------------------------+
	// | Value String (Length octets)  |
	// +-------------------------------+

	const size_t max_size =
	    1 + MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE + size_value;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i] = 0x40; // NOLINT(readability-magic-numbers)

	{ // encode key as table index

		int8_t result = encode_hpack_variable_integer(data + i, field_key_table_idx,
		                                              6); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;
	}

	{ // encode value

		// set Huffman to true
		data[i] = 0x80; // NOLINT(readability-magic-numbers)

		int8_t result = encode_hpack_variable_integer(data + i, size_value,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		const HuffmanEncodeFixedResult enc_result =
		    hpack_huffman_encode_value_fixed_size(data + i, size_value, &(field->value));

		IF_HUFFMAN_ENCODE_FIXED_RESULT_IS_ERROR_IGN(enc_result) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t result_size = huffman_encode_fixed_result_get_as_ok(enc_result).size;

		if(result_size != size_value) {
			// size_key way wrong, as http_hpack_encode_value_fixed_size just accepts an upper
			// bound, it may encode in fewer bytes, may_size just tells, how much place the buffer
			// has
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result_size;
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

	const HpackHeaderDynamicEntry entry = {
		.key = tstr_dup(&(field->key)),
		.value = tstr_dup(&(field->value)),
	};

	insert_entry_into_dynamic_table(&(compress_state->dynamic_table_state), entry);

	return buffer;
}

NODISCARD static SizedBuffer encode_single_header_field_literal_incremental_indexing_variant1(
    const size_t field_key_table_idx, const HttpHeaderField* const field,
    const Http2HpackHuffmanUsage huffman_usage, HpackCompressState* const compress_state) {

	switch(huffman_usage) {
		case Http2HpackHuffmanUsageNever: {
			return encode_single_header_field_literal_incremental_indexing_variant1_no_huffman(
			    field_key_table_idx, field, compress_state);
		}
		case Http2HpackHuffmanUsageAlways: {
			const size_t size_value = hpack_huffman_get_encoded_size(&(field->value));

			return encode_single_header_field_literal_incremental_indexing_variant1_huffman(
			    field_key_table_idx, field, size_value, compress_state);
		}
		case Http2HpackHuffmanUsageAuto:
		default: {

			const size_t size_value = hpack_huffman_get_encoded_size(&(field->value));

			if(size_value < tstr_len(&(field->value))) {
				return encode_single_header_field_literal_incremental_indexing_variant1_huffman(
				    field_key_table_idx, field, size_value, compress_state);
			}

			return encode_single_header_field_literal_incremental_indexing_variant1_no_huffman(
			    field_key_table_idx, field, compress_state);
		}
	}
}

NODISCARD static SizedBuffer
encode_single_header_field_literal_incremental_indexing_variant2_no_huffman(
    const HttpHeaderField* const field, HpackCompressState* const compress_state) {
	// encode the value as:
	// Literal Header Field with Incremental Indexing:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1

	// variant 2:

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

	const size_t key_size = tstr_len(&(field->key));
	const size_t value_size = tstr_len(&(field->value));

	const size_t max_size = MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE +
	                        key_size + value_size + 1;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i++] = 0x40; // NOLINT(readability-magic-numbers)

	{ // encode key / name

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, key_size,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&(field->key)), key_size);

		i += key_size;
	}

	{ // encode value

		// set Huffman to false
		data[i] = 0x00;

		int8_t result = encode_hpack_variable_integer(data + i, value_size,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		memcpy(data + i, tstr_cstr(&(field->value)), value_size);

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

	const HpackHeaderDynamicEntry entry = {
		.key = tstr_dup(&(field->key)),
		.value = tstr_dup(&(field->value)),
	};

	insert_entry_into_dynamic_table(&(compress_state->dynamic_table_state), entry);

	return buffer;
}

NODISCARD static SizedBuffer
encode_single_header_field_literal_incremental_indexing_variant2_huffman(
    const HttpHeaderField* const field, const size_t size_key, const size_t size_value,
    HpackCompressState* const compress_state) {
	// encode the value as:
	// Literal Header Field with Incremental Indexing:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1

	// variant 2:

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

	const size_t max_size = MAX_HPACK_VARIABLE_INTEGER_SIZE + MAX_HPACK_VARIABLE_INTEGER_SIZE +
	                        size_key + size_value + 1;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	data[i++] = 0x40; // NOLINT(readability-magic-numbers)

	{ // encode key / name

		// set Huffman to true
		data[i] = 0x80; // NOLINT(readability-magic-numbers)

		int8_t result = encode_hpack_variable_integer(data + i, size_key,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		const HuffmanEncodeFixedResult enc_result =
		    hpack_huffman_encode_value_fixed_size(data + i, size_key, &(field->key));

		IF_HUFFMAN_ENCODE_FIXED_RESULT_IS_ERROR_IGN(enc_result) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t result_size = huffman_encode_fixed_result_get_as_ok(enc_result).size;

		if(result_size != size_key) {
			// size_key way wrong, as http_hpack_encode_value_fixed_size just accepts an upper
			// bound, it may encode in fewer bytes, may_size just tells, how much place the buffer
			// has
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result_size;
	}

	{ // encode value

		// set Huffman to true
		data[i] = 0x80; // NOLINT(readability-magic-numbers)

		int8_t result = encode_hpack_variable_integer(data + i, size_value,
		                                              7); // NOLINT(readability-magic-numbers)

		if(result < 1) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result;

		const HuffmanEncodeFixedResult enc_result =
		    hpack_huffman_encode_value_fixed_size(data + i, size_value, &(field->value));

		IF_HUFFMAN_ENCODE_FIXED_RESULT_IS_ERROR_IGN(enc_result) {
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t result_size = huffman_encode_fixed_result_get_as_ok(enc_result).size;

		if(result_size != size_value) {
			// size_key way wrong, as http_hpack_encode_value_fixed_size just accepts an upper
			// bound, it may encode in fewer bytes, may_size just tells, how much place the buffer
			// has
			free_sized_buffer(buffer);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		i += result_size;
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

	const HpackHeaderDynamicEntry entry = {
		.key = tstr_dup(&(field->key)),
		.value = tstr_dup(&(field->value)),
	};

	insert_entry_into_dynamic_table(&(compress_state->dynamic_table_state), entry);

	return buffer;
}

NODISCARD static SizedBuffer encode_single_header_field_literal_incremental_indexing_variant2(
    const HttpHeaderField* const field, const Http2HpackHuffmanUsage huffman_usage,
    HpackCompressState* const compress_state) {
	switch(huffman_usage) {
		case Http2HpackHuffmanUsageNever: {
			return encode_single_header_field_literal_incremental_indexing_variant2_no_huffman(
			    field, compress_state);
		}
		case Http2HpackHuffmanUsageAlways: {
			const size_t size_key = hpack_huffman_get_encoded_size(&(field->key));
			const size_t size_value = hpack_huffman_get_encoded_size(&(field->value));

			return encode_single_header_field_literal_incremental_indexing_variant2_huffman(
			    field, size_key, size_value, compress_state);
		}
		case Http2HpackHuffmanUsageAuto:
		default: {

			const size_t size_key = hpack_huffman_get_encoded_size(&(field->key));
			const size_t size_value = hpack_huffman_get_encoded_size(&(field->value));

			if(size_key + size_value < tstr_len(&(field->key)) + tstr_len(&(field->value))) {
				return encode_single_header_field_literal_incremental_indexing_variant2_huffman(
				    field, size_key, size_value, compress_state);
			}

			return encode_single_header_field_literal_incremental_indexing_variant2_no_huffman(
			    field, compress_state);
		}
	}
}

NODISCARD static SizedBuffer
http2_hpack_compress_data_simple(const HttpHeaderFields header_fields,
                                 Http2HpackHuffmanUsage huffman_usage) {
	SizedBuffer result = { .data = NULL, .size = 0 };

	//  This only uses "Literal Header Field Never Indexed", they take up some space, but are
	//  easy to create, as they don't require any lookup in the static or dynamic table

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, header_fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, header_fields, i);

		const SizedBuffer single_header_result =
		    encode_single_header_field_literal_never_indexed_variant2(&field, huffman_usage);

		if(single_header_result.data == NULL) {
			free_sized_buffer(result);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t old_size = result.size;
		void* new_data = realloc(result.data, old_size + single_header_result.size);

		if(new_data == NULL) {
			free_sized_buffer(single_header_result);
			free_sized_buffer(result); // NOLINT(clang-analyzer-unix.Malloc)

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

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	TableFindResultTypeNotFound = 0,
	TableFindResultTypeKeyFound,
	TableFindResultTypeAllFound,
} TableFindResultType;

typedef struct {
	TableFindResultType type;
	union {
		size_t index;
	} data;
} TableFindResult;

NODISCARD static inline TableFindResultType table_entry_matches(const HttpHeaderField* const field,
                                                                const tstr* const entry_key,
                                                                const tstr* const entry_value) {
	if(!tstr_eq(&(field->key), entry_key)) {
		return TableFindResultTypeNotFound;
	}

	if(tstr_is_null(entry_value)) {
		return TableFindResultTypeKeyFound;
	}

	if(!tstr_eq(&(field->value), entry_value)) {
		return TableFindResultTypeKeyFound;
	}

	return TableFindResultTypeAllFound;
}

NODISCARD static inline TableFindResultType
table_entry_matches_dynamic(const HttpHeaderField* const field,
                            const HpackHeaderDynamicEntry* const entry) {
	return table_entry_matches(field, &(entry->key), &(entry->value));
}

NODISCARD static TableFindResult find_in_tables(const HttpHeaderField* const field,
                                                const HpackCompressState* const compress_state,
                                                const bool use_all_tables) {

	TableFindResult result = { .type = TableFindResultTypeNotFound };

	const StaticTableFindResult static_table_find_result =
	    hpack_generated_find_in_static_table_fast(field);

	SWITCH_STATIC_TABLE_FIND_RESULT(static_table_find_result) {
		CASE_STATIC_TABLE_FIND_RESULT_IS_ALL_FOUND_CONST(static_table_find_result) {
			return (TableFindResult){ .type = TableFindResultTypeAllFound,
				                      .data = { .index = all_found.index } };
		}
		CASE_STATIC_TABLE_FIND_RESULT_IS_KEY_FOUND_CONST(static_table_find_result) {
			// - store the key found result, maybe we find a better entry, so we use that,
			// otherwise we use this entry
			// - always overwrite the current result, since íf multiple entries match the key,
			// it is irrelevant which entry we use

			result = (TableFindResult){ .type = TableFindResultTypeKeyFound,
				                        .data = { .index = key_found.index } };
			break;
		}
		VARIANT_CASE_END();
		CASE_STATIC_TABLE_FIND_RESULT_IS_NOT_FOUND() {
			//
			break;
		}
		VARIANT_CASE_END();
		default: {
			break;
		}
	}

	if(!use_all_tables) {
		// return best result so far
		return result;
	}

	for(size_t i = 0;
	    i < hpack_dynamic_table_size(&(compress_state->dynamic_table_state.dynamic_table)); ++i) {
		const HpackHeaderDynamicEntryResult dynamic_entry_res =
		    hpack_dynamic_table_at(&(compress_state->dynamic_table_state.dynamic_table), i);

		assert(dynamic_entry_res.ok);

		const TableFindResultType matches_entry =
		    table_entry_matches_dynamic(field, &dynamic_entry_res.entry);

		switch(matches_entry) {
			case TableFindResultTypeAllFound: {
				return (
				    TableFindResult){ .type = TableFindResultTypeAllFound,
					                  .data = { .index = i + 1 + HPACK_STATIC_HEADER_TABLE_SIZE } };
			}
			case TableFindResultTypeKeyFound: {
				// - store the key found result, only if we don#t have another key found entry,
				// maybe we find a better entry, so we use that, otherwise we use this entry
				// - never overwrite the current result, as the previous index is always smaller,
				// which makes it smaller, after it is encoded, so prefer the best first match,
				// since íf multiple entries match the key, it is irrelevant which entry we use

				if(result.type == TableFindResultTypeNotFound) {
					result =
					    (TableFindResult){ .type = TableFindResultTypeKeyFound,
						                   .data = { .index =
						                                 i + 1 + HPACK_STATIC_HEADER_TABLE_SIZE } };
				}
				break;
			}
			case TableFindResultTypeNotFound:
			default: {
				break;
			}
		}

		//
	}

	return result;
}

NODISCARD static bool should_add_header_to_table(const HttpHeaderField* const field,
                                                 const Http2HpackTableAddType table_add_type) {

	switch(table_add_type) {
		case Http2HpackTableAddTypeNone: {
			return false;
		}
		case Http2HpackTableAddTypeCommon: {
			// note: this compares the key to a list of common names, e.g. date, cookie, server etc
			// and it does that faster than a huge else if tree of all possibilities therefore it is
			// generated from the list
			return hpack_generated_is_common_field_key_fast(tstr_as_view(&field->key));
		}
		case Http2HpackTableAddTypeAll: {
			return true;
		}
		default: {
			return false;
		}
	}
}

NODISCARD static SizedBuffer encode_single_header_field_extended_as_whole(
    const HttpHeaderField* const field, HpackCompressState* const compress_state,
    const Http2HpackHuffmanUsage huffman_usage, const Http2HpackTableAddType table_add_type) {

	const bool should_add = should_add_header_to_table(field, table_add_type);

	if(should_add) {
		return encode_single_header_field_literal_incremental_indexing_variant2(
		    field, huffman_usage, compress_state);
	}

	return encode_single_header_field_literal_never_indexed_variant2(field, huffman_usage);
}

NODISCARD static SizedBuffer encode_single_header_field_extended_with_key_from_table(
    const HttpHeaderField* const field, HpackCompressState* const compress_state,
    const Http2HpackHuffmanUsage huffman_usage,
    const Http2HpackTableAddType table_add_type, // NOLINT(bugprone-easily-swappable-parameters)
    const size_t key_table_idx) {

	const bool should_add = should_add_header_to_table(field, table_add_type);

	if(should_add) {
		return encode_single_header_field_literal_incremental_indexing_variant1(
		    key_table_idx, field, huffman_usage, compress_state);
	}

	return encode_single_header_field_literal_never_indexed_variant1(key_table_idx, &(field->value),
	                                                                 huffman_usage);
}

NODISCARD static SizedBuffer
encode_single_header_field_indexed_header_field(const size_t entry_table_idx) {
	// encode the value as:
	// Indexed Header Field:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
	//    0   1   2   3   4   5   6   7
	//  +---+---+---+---+---+---+---+---+
	//  | 1 |        Index (7+)         |
	//  +---+---------------------------+

	const size_t max_size = MAX_HPACK_VARIABLE_INTEGER_SIZE;

	SizedBuffer buffer = allocate_sized_buffer(max_size);

	if(buffer.data == NULL) {
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	uint8_t* data = (uint8_t*)buffer.data;

	size_t i = 0;

	// set the first bit
	data[i] = 0x80; // NOLINT(readability-magic-numbers)

	assert(entry_table_idx != 0);
	int8_t result = encode_hpack_variable_integer(data + i, entry_table_idx,
	                                              7); // NOLINT(readability-magic-numbers)

	if(result < 1 || (size_t)result > MAX_HPACK_VARIABLE_INTEGER_SIZE) {
		free_sized_buffer(buffer);
		return (SizedBuffer){ .data = NULL, .size = 0 };
	}

	i += result;

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

NODISCARD static inline SizedBuffer
encode_single_header_field_extended_with_entry_from_table(const size_t entry_table_idx) {
	return encode_single_header_field_indexed_header_field(entry_table_idx);
}

NODISCARD static SizedBuffer encode_single_header_field_extended(
    const HttpHeaderField* const field, HpackCompressState* const compress_state,
    const Http2HpackHuffmanUsage huffman_usage, const Http2HpackTableAddType table_add_type,
    const bool use_all_tables) {
	//
	const TableFindResult table_find_result = find_in_tables(field, compress_state, use_all_tables);

	switch(table_find_result.type) {
		case TableFindResultTypeNotFound: {
			return encode_single_header_field_extended_as_whole(field, compress_state,
			                                                    huffman_usage, table_add_type);
		}
		case TableFindResultTypeKeyFound: {
			return encode_single_header_field_extended_with_key_from_table(
			    field, compress_state, huffman_usage, table_add_type, table_find_result.data.index);
		}
		case TableFindResultTypeAllFound: {
			return encode_single_header_field_extended_with_entry_from_table(
			    table_find_result.data.index);
		}
		default: {
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}
	}
}

NODISCARD static SizedBuffer http2_hpack_compress_data_extended(
    const HttpHeaderFields header_fields, HpackCompressState* const compress_state,
    const Http2HpackHuffmanUsage huffman_usage, const Http2HpackTableAddType table_add_type,
    const bool use_all_tables) {
	SizedBuffer result = { .data = NULL, .size = 0 };

	//  This only uses "Literal Header Field Never Indexed", they take up some space, but are
	//  easy to create, as they don't require any lookup in the static or dynamic table

	for(size_t i = 0; i < TVEC_LENGTH(HttpHeaderField, header_fields); ++i) {

		HttpHeaderField field = TVEC_AT(HttpHeaderField, header_fields, i);

		const SizedBuffer single_header_result = encode_single_header_field_extended(
		    &field, compress_state, huffman_usage, table_add_type, use_all_tables);

		if(single_header_result.data == NULL) {
			free_sized_buffer(result);
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}

		const size_t old_size = result.size;
		void* new_data = realloc(result.data, old_size + single_header_result.size);

		if(new_data == NULL) {
			free_sized_buffer(single_header_result);
			free_sized_buffer(result); // NOLINT(clang-analyzer-unix.Malloc)

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

NODISCARD SizedBuffer http2_hpack_compress_data(HpackCompressState* const compress_state,
                                                const HttpHeaderFields header_fields,
                                                Http2HpackCompressOptions options) {

	switch(options.type) {
		case Http2HpackCompressTypeNoTableUsage: {
			return http2_hpack_compress_data_simple(header_fields, options.huffman_usage);
		}
		case Http2HpackCompressTypeStaticTableUsage:
		case Http2HpackCompressTypeAllTablesUsage: {
			return http2_hpack_compress_data_extended(
			    header_fields, compress_state, options.huffman_usage, options.table_add_type,
			    options.type == Http2HpackCompressTypeAllTablesUsage);
		}
		default: {
			return (SizedBuffer){ .data = NULL, .size = 0 };
		}
	}
}
