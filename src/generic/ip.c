#include "./ip.h"

#include <arpa/inet.h>

#include "generic/serialize.h"

NODISCARD IPAddress from_ipv4(struct in_addr address) {

	return (IPAddress){ .version = IPProtocolVersionV4,
		                .data = {
		                    .v4 = (IPV4Address){ .underlying = address },
		                } };
}

NODISCARD IPAddress from_ipv6(struct in6_addr address) {
	return (IPAddress){ .version = IPProtocolVersionV6,
		                .data = {
		                    .v6 = (IPV6Address){ .underlying = address },
		                } };
}

NODISCARD char* ipv4_to_string(IPV4Address v4_address) {

	// +1 for the 0 byte
	char* str = malloc(INET_ADDRSTRLEN);

	if(str == NULL) {
		return NULL;
	}

	const char* res = inet_ntop(AF_INET, &v4_address.underlying, str, INET_ADDRSTRLEN);

	if(res == NULL) {
		free(str);
		return NULL;
	}

	return str;
}

NODISCARD char* ipv6_to_string(IPV6Address v6_address) {

	// +1 for the 0 byte
	char* str = malloc(INET6_ADDRSTRLEN);

	if(str == NULL) {
		return NULL;
	}

	const char* res = inet_ntop(AF_INET6, &v6_address.underlying, str, INET6_ADDRSTRLEN);

	if(res == NULL) {
		free(str);
		return NULL;
	}

	return str;
}

NODISCARD char* ipv_to_string(IPAddress address) {

	switch(address.version) {
		case IPProtocolVersionV4: {
			return ipv4_to_string(address.data.v4);
		}
		case IPProtocolVersionV6: {
			return ipv6_to_string(address.data.v6);
		}
		default: {
			return NULL;
		}
	}
}

NODISCARD IPV4Address get_ipv4_address_from_host_bytes(const uint8_t* bytes) {

	uint32_t addr = deserialize_u32_le_to_no(bytes);

	struct in_addr value = { .s_addr = addr };

	return (IPV4Address){ .underlying = value };
}

NODISCARD IPV4RawBytes get_raw_bytes_as_host_bytes_from_ipv4_address(IPV4Address address) {

	SerializeResult32 result = serialize_u32_no_to_host(address.underlying.s_addr);

	return (IPV4RawBytes){ .bytes = {
		                       result.bytes[0],
		                       result.bytes[1],
		                       result.bytes[2],
		                       result.bytes[3],
		                   } };
}
