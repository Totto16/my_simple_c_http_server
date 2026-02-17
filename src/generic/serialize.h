#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/utils.h"

/**
 * @brief Abbreviations:
 * le => Little endian
 * no => network order
 * be => big endian (same as no)
 * host => host byte order
 */

NODISCARD uint16_t deserialize_u16_le_to_no(const uint8_t* bytes);

NODISCARD uint32_t deserialize_u32_le_to_no(const uint8_t* bytes);

NODISCARD uint16_t deserialize_u16_le_to_host(const uint8_t* bytes);

NODISCARD uint32_t deserialize_u32_le_to_host(const uint8_t* bytes);

NODISCARD uint16_t deserialize_u16_be_to_host(const uint8_t* bytes);

NODISCARD uint32_t deserialize_u32_be_to_host(const uint8_t* bytes);

NODISCARD uint64_t deserialize_u64_be_to_host(const uint8_t* bytes);

typedef struct {
	uint8_t bytes[2];
} SerializeResult16;

NODISCARD SerializeResult16 serialize_u16_no_to_host(uint16_t bytes);

NODISCARD SerializeResult16 serialize_u16_host_to_be(uint16_t bytes);

typedef struct {
	uint8_t bytes[4];
} SerializeResult32;

NODISCARD SerializeResult32 serialize_u32_no_to_host(uint32_t bytes);

NODISCARD SerializeResult32 serialize_u32_host_to_be(uint32_t bytes);

typedef struct {
	uint8_t bytes[8];
} SerializeResult64;

NODISCARD SerializeResult64 serialize_u64_host_to_be(uint64_t bytes);

#ifdef __cplusplus
}
#endif
