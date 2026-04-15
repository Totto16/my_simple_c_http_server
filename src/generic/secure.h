
#pragma once

#ifndef _SIMPLE_SERVER_SECURE_DISABLED
	#include <openssl/ssl.h>
#endif

#include "utils/utils.h"

#include <tstr.h>
#include <tvec.h>

#include "utils/sized_buffer.h"

typedef struct SecureDataImpl SecureData;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	SecureOptionsTypeNotSecure = 0,
	SecureOptionsTypeSecure,
} SecureOptionsType;

typedef struct {
	SecureOptionsType type;
	union {
		SecureData* data;
	} value;
} SecureOptions;

typedef struct ConnectionContextImpl ConnectionContext;

TVEC_DEFINE_VEC_TYPE_EXTENDED(ConnectionContext*, ConnectionContextPtr)

typedef TVEC_TYPENAME(ConnectionContextPtr) ConnectionContextPtrs;

typedef struct ConnectionDescriptorImpl ConnectionDescriptor;

NODISCARD bool is_secure(const SecureOptions* options);

NODISCARD bool is_secure_context(const ConnectionContext* context);

NODISCARD SecureOptions* initialize_secure_options(bool secure, tstr_static public_cert_file,
                                                   tstr_static private_cert_file);

void free_secure_options(SecureOptions* options);

NODISCARD ConnectionContext* get_connection_context(const SecureOptions* options);

NODISCARD ConnectionContext* copy_connection_context(const ConnectionContext* old_context);

void free_connection_context(ConnectionContext* context);

typedef LibCInt NativeFd;

NODISCARD ConnectionDescriptor* get_connection_descriptor(const ConnectionContext* context,
                                                          NativeFd native_fd);

NODISCARD GenericResult close_connection_descriptor(ConnectionDescriptor* descriptor);

NODISCARD GenericResult close_connection_descriptor_advanced(ConnectionDescriptor* descriptor,
                                                             ConnectionContext* context,
                                                             bool allow_reuse);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ReadResultTypeEOF = 0,
	ReadResultTypeSuccess,
	ReadResultTypeError,
} ReadResultType;

typedef union {
	LibCInt errno_error;
	unsigned long ssl_error; // NOLINT(totto-use-fixed-width-types-var)
} OpaqueError;

typedef struct {
	ReadResultType type;
	union {
		size_t bytes_read;
		OpaqueError opaque_error;
	} data;
} ReadResult;

NODISCARD ReadResult read_from_descriptor(const ConnectionDescriptor* descriptor, void* buffer,
                                          size_t n_bytes);

NODISCARD char* get_read_error_meaning(const ConnectionDescriptor* descriptor,
                                       OpaqueError opaque_error);

NODISCARD ssize_t write_to_descriptor(const ConnectionDescriptor* descriptor,
                                      ReadonlyBuffer buffer);

NODISCARD NativeFd get_underlying_socket(const ConnectionDescriptor* descriptor);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ProtocolSelectedNone = 0,
	ProtocolSelectedHttp1Dot1,
	ProtocolSelectedHttp2,
} ProtocolSelected;

NODISCARD ProtocolSelected get_selected_protocol(const ConnectionDescriptor* descriptor);
