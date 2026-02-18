

#pragma once

#include "utils/utils.h"

#include <netinet/in.h>
#include <sys/socket.h>
/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	IPProtocolVersionV4 = 0,
	IPProtocolVersionV6,
} IPProtocolVersion;

typedef struct {
	struct in_addr underlying; // this is in network order
} IPV4Address;

typedef struct {
	struct in6_addr underlying; // this is in network order
} IPV6Address;

typedef struct {
	IPProtocolVersion version;
	union {
		IPV4Address v4;
		IPV6Address v6;
	} data;
} IPAddress;

NODISCARD IPAddress from_ipv4(struct in_addr address);

NODISCARD IPAddress from_ipv6(struct in6_addr address);

NODISCARD char* ipv4_to_string(IPV4Address v4_address);

NODISCARD char* ipv6_to_string(IPV6Address v6_address);

NODISCARD char* ipv_to_string(IPAddress address);

typedef struct {
	uint8_t bytes[4];
} IPV4RawBytes;

NODISCARD IPV4Address get_ipv4_address_from_host_bytes(const uint8_t* bytes);

NODISCARD IPV4RawBytes get_raw_bytes_as_host_bytes_from_ipv4_address(IPV4Address address);
