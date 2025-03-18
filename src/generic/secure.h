
#pragma once

#include <openssl/ssl.h>

#include "utils/utils.h"

#define ESSL 167

typedef struct SecureDataImpl SecureData;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	SECURE_OPTIONS_TYPE_NOT_SECURE = 0,
	SECURE_OPTIONS_TYPE_SECURE,
} SECURE_OPTIONS_TYPE;

typedef struct {
	SECURE_OPTIONS_TYPE type;
	union {
		SecureData* data;
	} data;
} SecureOptions;

typedef struct ConnectionContextImpl ConnectionContext;

typedef struct ConnectionDescriptorImpl ConnectionDescriptor;

bool is_secure(const SecureOptions* options);

bool is_secure_context(const ConnectionContext* context);

SecureOptions* initialize_secure_options(bool secure, const char* public_cert_file,
                                         const char* private_cert_file);

void free_secure_options(SecureOptions* options);

ConnectionContext* get_connection_context(const SecureOptions* options);

ConnectionContext* copy_connection_context(const ConnectionContext* old_context);

void free_connection_context(ConnectionContext* context);

ConnectionDescriptor* get_connection_descriptor(const ConnectionContext* context, int native_fd);

int close_connection_descriptor(ConnectionDescriptor* descriptor);

int close_connection_descriptor_advanced(ConnectionDescriptor* descriptor,
                                         ConnectionContext* context, bool allow_reuse);

int read_from_descriptor(const ConnectionDescriptor* descriptor, void* buffer, size_t n_bytes);

ssize_t write_to_descriptor(const ConnectionDescriptor* descriptor, void* buffer, size_t n_bytes);

int get_underlying_socket(const ConnectionDescriptor* descriptor);
