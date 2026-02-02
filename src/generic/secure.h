
#pragma once

#include <openssl/ssl.h>

#include "utils/utils.h"

#include <zvec/zvec.h>

#define ESSL 167

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
	} data;
} SecureOptions;

typedef struct ConnectionContextImpl ConnectionContext;

ZVEC_DEFINE_VEC_TYPE_EXTENDED(ConnectionContext*, ConnectionContextPtr)

typedef ZVEC_TYPENAME(ConnectionContextPtr) ConnectionContextPtrs;

typedef struct ConnectionDescriptorImpl ConnectionDescriptor;

NODISCARD bool is_secure(const SecureOptions* options);

NODISCARD bool is_secure_context(const ConnectionContext* context);

NODISCARD SecureOptions* initialize_secure_options(bool secure, const char* public_cert_file,
                                                   const char* private_cert_file);

void free_secure_options(SecureOptions* options);

NODISCARD ConnectionContext* get_connection_context(const SecureOptions* options);

NODISCARD ConnectionContext* copy_connection_context(const ConnectionContext* old_context);

void free_connection_context(ConnectionContext* context);

NODISCARD ConnectionDescriptor* get_connection_descriptor(const ConnectionContext* context,
                                                          int native_fd);

int close_connection_descriptor(ConnectionDescriptor* descriptor);

int close_connection_descriptor_advanced(ConnectionDescriptor* descriptor,
                                         ConnectionContext* context, bool allow_reuse);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ReadResultTypeEOF = 0,
	ReadResultTypeSuccess,
	ReadResultTypeError,
} ReadResultType;

typedef union {
	int errno_error;
	unsigned long ssl_error;
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

NODISCARD char* get_read_error_meaning(const ConnectionDescriptor* descriptor, OpaqueError opaque_error);

NODISCARD ssize_t write_to_descriptor(const ConnectionDescriptor* descriptor, void* buffer,
                                      size_t n_bytes);

NODISCARD int get_underlying_socket(const ConnectionDescriptor* descriptor);
