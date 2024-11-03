

#include "secure.h"
#include "utils/log.h"
#include "utils/utils.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <unistd.h>

struct SecureDataImpl {
	SSL_CTX* ssl_context;
};

struct ConnectionContextImpl {
	SECURE_OPTIONS_TYPE type;
	union {
		struct {
			SSL* ssl_structure;
			const SecureOptions* options;
		} data;
	} data;
};

struct ConnectionDescriptorImpl {
	SECURE_OPTIONS_TYPE type;
	union {
		SSL* ssl_structure;
		int fd;
	} data;
};

bool is_secure(const SecureOptions* const options) {
	return options->type == SECURE_OPTIONS_TYPE_SECURE;
}

static bool file_exists(const char* const file) {
	return access(file, R_OK) == 0;
}

// NOTE: this has to return the number of bytes written, if this returns <= 0, the error reporting
// stops, see
// https://github.com/openssl/openssl/blob/3b90a847ece93b3886f14adc7061e70456d564e1/crypto/err/err_prn.c#L44
static int error_logger(const char* str, size_t len, void* user_data) {

	UNUSED(user_data);

	LOG_MESSAGE(LogLevelError, "\t%*s\n", (int)len, str);
	return len;
}

static SecureData* initialize_secure_data(const char* const public_cert_file,
                                          const char* const private_cert_file) {

	if(!file_exists(public_cert_file)) {
		LOG_MESSAGE(LogLevelError, "public_cert_file '%s' doesn't exist\n", public_cert_file);
		return NULL;
	}

	if(!file_exists(private_cert_file)) {
		LOG_MESSAGE(LogLevelError, "private_cert_file '%s' doesn't exist\n", private_cert_file);
		return NULL;
	}

	SSL_load_error_strings(); /* readable error messages */
	SSL_library_init();       /* initialize library */

	SecureData* data = mallocOrFail(sizeof(SecureData), true);

	SSL_CTX* ssl_context = SSL_CTX_new(TLS_server_method());
	if(ssl_context == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_new failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	int result = SSL_CTX_use_certificate_file(ssl_context, public_cert_file, SSL_FILETYPE_PEM);
	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_use_certificate_file failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	result = SSL_CTX_use_PrivateKey_file(ssl_context, private_cert_file, SSL_FILETYPE_PEM);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_CTX_use_PrivateKey_file failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	result = SSL_CTX_check_private_key(ssl_context);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_CTX_check_private_key failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	data->ssl_context = ssl_context;

	return data;
}

static void free_secure_data(SecureData* data) {

	SSL_CTX_free(data->ssl_context);
	free(data);
}

SecureOptions* initialize_secure_options(bool secure, const char* const public_cert_file,
                                         const char* const private_cert_file) {

	SecureOptions* options = mallocOrFail(sizeof(SecureOptions), true);

	if(!secure) {
		options->type = SECURE_OPTIONS_TYPE_NOT_SECURE;
		return options;
	}

	options->type = SECURE_OPTIONS_TYPE_SECURE;

	SecureData* data = initialize_secure_data(public_cert_file, private_cert_file);

	if(data == NULL) {
		return NULL;
	}

	options->data.data = data;

	return options;
}

void free_secure_options(SecureOptions* const options) {

	if(!is_secure(options)) {
		free(options);
		return;
	}

	free_secure_data(options->data.data);
	free(options);
}

bool is_secure_context(const ConnectionContext* const context) {
	return context->type == SECURE_OPTIONS_TYPE_SECURE;
}

static SSL* new_ssl_structure_from_ctx(SSL_CTX* ssl_context) {
	SSL* ssl_structure = SSL_new(ssl_context);
	if(ssl_structure == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_new failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	return ssl_structure;
}

ConnectionContext* get_connection_context(const SecureOptions* const options) {

	ConnectionContext* context = mallocOrFail(sizeof(ConnectionContext), true);

	if(!is_secure(options)) {
		context->type = SECURE_OPTIONS_TYPE_NOT_SECURE;
		return context;
	}

	context->type = SECURE_OPTIONS_TYPE_SECURE;

	SecureData* data = options->data.data;

	SSL* ssl_structure = new_ssl_structure_from_ctx(data->ssl_context);

	if(ssl_structure == NULL) {
		return NULL;
	}

	context->data.data.ssl_structure = ssl_structure;
	context->data.data.options = options;

	return context;
}

ConnectionContext* copy_connection_context(const ConnectionContext* const old_context) {

	ConnectionContext* context = mallocOrFail(sizeof(ConnectionContext), true);

	if(!is_secure_context(old_context)) {
		context->type = SECURE_OPTIONS_TYPE_NOT_SECURE;
		return context;
	}

	context->type = SECURE_OPTIONS_TYPE_SECURE;

	const SecureOptions* const options = old_context->data.data.options;

	SecureData* data = options->data.data;

	SSL* ssl_structure = new_ssl_structure_from_ctx(data->ssl_context);

	if(ssl_structure == NULL) {
		return NULL;
	}

	context->data.data.ssl_structure = ssl_structure;
	context->data.data.options = options;

	return context;
}

void free_connection_context(ConnectionContext* context) {

	if(!is_secure_context(context)) {
		free(context);
		return;
	}

	SSL_free(context->data.data.ssl_structure);

	free(context);
}

static bool is_secure_descriptor(const ConnectionDescriptor* const descriptor) {
	return descriptor->type == SECURE_OPTIONS_TYPE_SECURE;
}

ConnectionDescriptor* get_connection_descriptor(const ConnectionContext* const context, int fd) {

	ConnectionDescriptor* descriptor = mallocOrFail(sizeof(ConnectionDescriptor), true);

	if(!is_secure_context(context)) {
		descriptor->type = SECURE_OPTIONS_TYPE_NOT_SECURE;
		descriptor->data.fd = fd;
		return descriptor;
	}

	descriptor->type = SECURE_OPTIONS_TYPE_SECURE;

	SSL* ssl_structure = context->data.data.ssl_structure;

	int result = SSL_set_fd(ssl_structure, fd);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_set_fd failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	int ssl_result = SSL_accept(ssl_structure);

	if(ssl_result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_accept failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	descriptor->data.ssl_structure = ssl_structure;

	return descriptor;
}

int close_connection_descriptor(const ConnectionDescriptor* const descriptor,
                                ConnectionContext* const context) {

	if(!is_secure_descriptor(descriptor)) {
		return close(descriptor->data.fd);
	}

	SSL* ssl_structure = descriptor->data.ssl_structure;

	int result = 0;

	// do a bidirectional shutdown, if SSL_shutdown returns 0, we have to repeat the call, if it
	// returns 1, we where successful
	do {
		result = SSL_shutdown(ssl_structure);

		if(result != 0 && result != 1) {
			// if we already shutdown on our side, it is not strictly speaking an error, the other
			// side did not terminate correctly, but we shouldn't error out because of that

			int shutdown_flags = SSL_get_shutdown(ssl_structure);

			if((shutdown_flags & SSL_SENT_SHUTDOWN) != 0) {
				break;
			}

			LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_shutdown failed:\n");
			ERR_print_errors_cb(error_logger, NULL);
			errno = ESSL;
			return -1;
		}

	} while(result == 0);

	// check the way the connection was closed, either both ends closed it correctly, or the other
	// side didn't clean up correctly
	int shutdown_flags = SSL_get_shutdown(ssl_structure);
	bool was_closed_correctly = false;
	bool allow_reuse = false;

	if((shutdown_flags & SSL_SENT_SHUTDOWN) != 0 && (shutdown_flags & SSL_RECEIVED_SHUTDOWN) != 0) {
		was_closed_correctly = true;
	}

	// if it was closed correctly, we can reuse the connection, otherwise we can't
	if(was_closed_correctly && allow_reuse) {

		// TODO: Warnings: allow_reuse should be configurable by keepalive connections:
		/* SSL_clear() resets the SSL object to allow for another connection. The reset operation
		 * however keeps several settings of the last sessions (some of these settings were made
		 * automatically during the last handshake). It only makes sense when opening a new session
		 * (or reusing an old one) with the same peer that shares these settings. SSL_clear() is not
		 * a short form for the sequence ssl_free(3); ssl_new(3); .  */
		result = SSL_clear(ssl_structure);

		if(result != 1) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "Error: SSL_clear failed:\n");
			ERR_print_errors_cb(error_logger, NULL);
			errno = ESSL;
			return -1;
		}

	} else {
		SSL_free(ssl_structure);

		assert(context->data.data.ssl_structure == ssl_structure);
		context->data.data.ssl_structure =
		    new_ssl_structure_from_ctx(context->data.data.options->data.data->ssl_context);

		if(context->data.data.ssl_structure == NULL) {
			errno = ESSL;
			return -1;
		}
	}

	return 0;
}

int read_from_descriptor(const ConnectionDescriptor* const descriptor, void* buffer,
                         size_t n_bytes) {
	if(!is_secure_descriptor(descriptor)) {
		return read(descriptor->data.fd, buffer, n_bytes);
	}

	SSL* ssl_structure = descriptor->data.ssl_structure;

	return SSL_read(ssl_structure, buffer, n_bytes);
}

ssize_t write_to_descriptor(const ConnectionDescriptor* const descriptor, void* buffer,
                            size_t n_bytes) {

	if(!is_secure_descriptor(descriptor)) {
		return write(descriptor->data.fd, buffer, n_bytes);
	}

	SSL* ssl_structure = descriptor->data.ssl_structure;

	return SSL_write(ssl_structure, buffer, n_bytes);
}
