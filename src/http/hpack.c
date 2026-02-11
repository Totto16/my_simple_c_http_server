#include "./hpack.h"

// see: https://datatracker.ietf.org/doc/html/rfc7541

static void parse_hpack_indexed_header_field(const uint8_t byte, size_t* pos, const size_t size,
                                             const uint8_t* const data) {
	// Indexed Header Field:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
	//    0   1   2   3   4   5   6   7
	//  +---+---+---+---+---+---+---+---+
	//  | 1 |        Index (7+)         |
	//  +---+---------------------------+

	// TODO
	UNREACHABLE();
}

static void parse_hpack_literal_header_field_with_incremental_indexing(const uint8_t byte,
                                                                       size_t* pos,
                                                                       const size_t size,
                                                                       const uint8_t* const data) {
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
	UNREACHABLE();
}

static void parse_hpack_dynamic_table_size_update(const uint8_t byte, size_t* pos,
                                                  const size_t size, const uint8_t* const data) {
	// Dynamic Table Size Update:
	// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
	//   0   1   2   3   4   5   6   7
	// +---+---+---+---+---+---+---+---+
	// | 0 | 0 | 1 |   Max size (5+)   |
	// +---+---------------------------+

	// TODO
	UNREACHABLE();
}

static void parse_hpack_literal_header_field_never_indexed(const uint8_t byte, size_t* pos,
                                                           const size_t size,
                                                           const uint8_t* const data) {
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
	UNREACHABLE();
}

static void parse_hpack_literal_header_field_without_indexing(const uint8_t byte, size_t* pos,
                                                              const size_t size,
                                                              const uint8_t* const data) {
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
	UNREACHABLE();
}

NODISCARD static SizedBuffer http2_hpack_decompress_data_impl(const SizedBuffer input) {

	size_t pos = 0;
	const size_t size = input.size;

	const uint8_t* const data = (uint8_t*)input.data;

	while(pos < size) {
		uint8_t byte = data[pos];

		if((byte & 0x80) != 0) {
			// Indexed Header Field:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.1
			//    0   1   2   3   4   5   6   7
			//  +---+---+---+---+---+---+---+---+
			//  | 1 |        Index (7+)         |
			//  +---+---------------------------+
			++pos;
			parse_hpack_indexed_header_field(byte & 0x7f, &pos, size, data);
		} else if((byte & 0xC0) == 0x40) {
			// Literal Header Field with Incremental Indexing:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.1
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 1 |      Index (6+)       |
			// +---+---+-----------------------+
			// ...
			++pos;
			parse_hpack_literal_header_field_with_incremental_indexing(byte & 0x3f, &pos, size,
			                                                           data);
		} else if((byte & 0xE0) == 0x20) {
			// Dynamic Table Size Update:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.3
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 1 |   Max size (5+)   |
			// +---+---------------------------+
			++pos;
			parse_hpack_dynamic_table_size_update(byte & 0x1f, &pos, size, data);
		} else if((byte & 0xF0) == 0x10) {
			// Literal Header Field Never Indexed:
			// https://datatracker.ietf.org/doc/html/rfc7541#section-6.2.3
			//   0   1   2   3   4   5   6   7
			// +---+---+---+---+---+---+---+---+
			// | 0 | 0 | 0 | 1 |  Index (4+)   |
			// +---+---+-----------------------+
			// ...
			++pos;
			parse_hpack_literal_header_field_never_indexed(byte & 0x0f, &pos, size, data);
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
			++pos;
			parse_hpack_literal_header_field_without_indexing(byte & 0x0f, &pos, size, data);
		}
	}
}

NODISCARD SizedBuffer http2_hpack_decompress_data(const SizedBuffer input) {

	return http2_hpack_decompress_data_impl(input)
}
