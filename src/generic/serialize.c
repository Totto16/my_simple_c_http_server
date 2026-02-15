
#include "./serialize.h"

#include <arpa/inet.h>
#include <endian.h>

#if defined(__APPLE__) || defined(__MACOSX__)
	#define bswap_16 __builtin_bswap16
	#define bswap_32 __builtin_bswap32
	#define bswap_64 __builtin_bswap64
#else
	#include <byteswap.h>
#endif

/**
 * @brief Abbreviations:
 * le => Little endian
 * no => network order
 * be => big endian (same as no)
 * host => host byte order
 */

#if BYTE_ORDER == LITTLE_ENDIAN
// 16
	#define LE_TO_HOST_16(bytes) bytes
	#define BE_TO_HOST_16(bytes) bswap_16(bytes)

	#define HOST_TO_BE_16(bytes) bswap_16(bytes)
	#define HOST_TO_LE_16(bytes) bytes
// 32
	#define LE_TO_HOST_32(bytes) bytes
	#define BE_TO_HOST_32(bytes) bswap_32(bytes)

	#define HOST_TO_BE_32(bytes) bswap_32(bytes)
	#define HOST_TO_LE_32(bytes) bytes
#elif BYTE_ORDER == BIG_ENDIAN
// 16
	#define LE_TO_HOST_16(bytes) bswap_16(bytes)
	#define BE_TO_HOST_16(bytes) bytes

	#define HOST_TO_BE_16(bytes) bytes
	#define HOST_TO_LE_16(bytes) bswap_16(bytes)
// 32
	#define LE_TO_HOST_32(bytes) bswap_32(bytes)
	#define BE_TO_HOST_32(bytes) bytes

	#define HOST_TO_BE_32(bytes) bytes
	#define HOST_TO_LE_32(bytes) bswap_32(bytes)
#else
	#error "Endianness not defined!"
#endif

NODISCARD static inline uint32_t deserialize_u32_host_to_host(const uint8_t* const bytes) {

	uint32_t value;
	memcpy(&value, bytes, sizeof(uint32_t));

	return value;
}

NODISCARD static inline uint16_t deserialize_u16_host_to_host(const uint8_t* const bytes) {

	uint16_t value;
	memcpy(&value, bytes, sizeof(uint16_t));

	return value;
}

NODISCARD uint16_t deserialize_u16_le_to_no(const uint8_t* const bytes) {
	const uint16_t raw_value = deserialize_u16_host_to_host(bytes);

	return htons(LE_TO_HOST_16(raw_value));
}

NODISCARD uint32_t deserialize_u32_le_to_no(const uint8_t* const bytes) {
	const uint32_t raw_value = deserialize_u32_host_to_host(bytes);

	return htonl(LE_TO_HOST_32(raw_value));
}

NODISCARD uint16_t deserialize_u16_le_to_host(const uint8_t* const bytes) {
	const uint16_t raw_value = deserialize_u16_host_to_host(bytes);

	return LE_TO_HOST_16(raw_value);
}

NODISCARD uint32_t deserialize_u32_le_to_host(const uint8_t* const bytes) {
	const uint32_t raw_value = deserialize_u32_host_to_host(bytes);

	return LE_TO_HOST_32(raw_value);
}

NODISCARD uint16_t deserialize_u16_be_to_host(const uint8_t* const bytes) {
	const uint16_t raw_value = deserialize_u16_host_to_host(bytes);

	return BE_TO_HOST_16(raw_value);
}

NODISCARD uint32_t deserialize_u32_be_to_host(const uint8_t* const bytes) {
	const uint32_t raw_value = deserialize_u32_host_to_host(bytes);

	return BE_TO_HOST_32(raw_value);
}

// serialize

NODISCARD static inline SerializeResult32 serialize_u32_host_to_host(const uint32_t bytes) {

	SerializeResult32 result;
	memcpy(&result.bytes, &bytes, sizeof(uint32_t));

	return result;
}

NODISCARD static inline SerializeResult16 serialize_u16_host_to_host(const uint16_t bytes) {

	SerializeResult16 result;
	memcpy(&result.bytes, &bytes, sizeof(uint16_t));

	return result;
}

NODISCARD SerializeResult32 serialize_u32_no_to_host(const uint32_t bytes) {
	const uint32_t raw_value = ntohl(bytes);

	return serialize_u32_host_to_host(raw_value);
}

NODISCARD SerializeResult32 serialize_u32_host_to_be(const uint32_t bytes) {
	const uint32_t raw_value = HOST_TO_BE_32(bytes);

	return serialize_u32_host_to_host(raw_value);
}

NODISCARD SerializeResult16 serialize_u16_no_to_host(const uint16_t bytes) {
	const uint16_t raw_value = ntohs(bytes);

	return serialize_u16_host_to_host(raw_value);
}

NODISCARD SerializeResult16 serialize_u16_host_to_be(const uint16_t bytes) {
	const uint16_t raw_value = HOST_TO_BE_16(bytes);

	return serialize_u16_host_to_host(raw_value);
}
