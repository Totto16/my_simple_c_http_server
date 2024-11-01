
#pragma once

#include <openssl/ssl.h>

#define ESSL 167

typedef struct SecureDataImpl SecureData;

typedef enum { SECURE_OPTIONS_TYPE_NOT_SECURE, SECURE_OPTIONS_TYPE_SECURE } SECURE_OPTIONS_TYPE;

typedef struct {
	SECURE_OPTIONS_TYPE type;
	union {
		SecureData* data;
	} data;
} SecureOptions;

typedef struct ConnectionContextImpl ConnectionContext;

typedef struct ConnectionDescriptorImpl ConnectionDescriptor;

bool is_secure(const SecureOptions* const options);

bool is_secure_context(const ConnectionContext* const context);

SecureOptions* initialize_secure_options(bool secure, const char* const public_cert_file,
                                         const char* const private_cert_file);

void free_secure_options(SecureOptions* const options);

ConnectionContext* get_connection_context(const SecureOptions* const options);

ConnectionContext* copy_connection_context(const ConnectionContext* const old_context);

void free_connection_context(ConnectionContext* context);

ConnectionDescriptor* get_connection_descriptor(const ConnectionContext* const context, int fd);

int close_connection_descriptor(const ConnectionDescriptor* const descriptor,
                                ConnectionContext* const context);

int read_from_descriptor(const ConnectionDescriptor* const descriptor, void* buffer,
                         size_t n_bytes);

ssize_t write_to_descriptor(const ConnectionDescriptor* const descriptor, void* buffer,
                            size_t n_bytes);
