

#include "secure.h"
#include "utils/log.h"
#include "utils/utils.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <unistd.h>

TVEC_IMPLEMENT_VEC_TYPE_EXTENDED(ConnectionContext*, ConnectionContextPtr)

// general notes: the openssl docs are quite extensive, even i didn't use them at the beginning, but
// there are also exaples:
// e.g.: https://docs.openssl.org/3.5/man7/ossl-guide-tls-server-block/#name
// docs url: https://docs.openssl.org/3.5/man7

struct SecureDataImpl {
	SSL_CTX* ssl_context;
};

struct ConnectionContextImpl {
	SecureOptionsType type;
	union {
		struct {
			SSL* ssl_structure;
			const SecureOptions* options;
		} data;
	} data;
};

typedef struct {
	SSL* ssl_structure;
	ProtocolSelected protocol;
} SecureConnectionData;

typedef struct {
	int fd;
} NormalConnectionData;

struct ConnectionDescriptorImpl {
	SecureOptionsType type;
	union {
		SecureConnectionData secure;
		NormalConnectionData normal;
	} data;
};

bool is_secure(const SecureOptions* const options) {
	return options->type == // NOLINT(readability-implicit-bool-conversion)
	       SecureOptionsTypeSecure;
}

#define ALPN_PROTO_H2 "h2"

#define H2_PROTO_SIZE 2

static_assert((sizeof(ALPN_PROTO_H2) / (sizeof(ALPN_PROTO_H2[0]))) - 1 == H2_PROTO_SIZE);

#define ALPN_PROTO_H1_1 "http/1.1"

#define H1_1_PROTO_SIZE 8

static_assert((sizeof(ALPN_PROTO_H1_1) / (sizeof(ALPN_PROTO_H1_1[0]))) - 1 == H1_1_PROTO_SIZE);

#ifndef _SIMPLE_SERVER_SECURE_DISABLED

static bool file_exists(const char* const file) {
	return access(file, R_OK) == 0; // NOLINT(readability-implicit-bool-conversion)
}

// NOTE: this has to return the number of bytes written, if this returns <= 0, the error reporting
// stops, see
// https://github.com/openssl/openssl/blob/3b90a847ece93b3886f14adc7061e70456d564e1/crypto/err/err_prn.c#L44
static int error_logger(const char* str, size_t len, void* user_data) {

	UNUSED(user_data);

	LOG_MESSAGE(LogLevelError, "\t%.*s\n", (int)len, str);
	return (int)len;
}

// NOTE: order is important, we advertise http/2 first :)
static const unsigned char g_alpn_protos[] = {
	H2_PROTO_SIZE,   'h', '2',                              // ALPN name for http/2
	H1_1_PROTO_SIZE, 'h', 't', 't', 'p', '/', '1', '.', '1' // ALPN name for http/1.1
};

// inspired by the docs and
// https://github.com/openssl/openssl/blob/master/demos/guide/quic-server-block.c#L95
static int alpn_select_cb(SSL* /* ssl */, const unsigned char** out, unsigned char* outlen,
                          const unsigned char* in, unsigned int inlen, void* /* arg */) {

	// check if the clinet support one of our protocols, if so, this fn return
	// OPENSSL_NPN_NEGOTIATED and sets out and outlen, so the exact thing the cb function expects,
	// so nothing to except return OK
	if(SSL_select_next_proto((unsigned char**)out, outlen, g_alpn_protos, sizeof(g_alpn_protos), in,
	                         inlen) == OPENSSL_NPN_NEGOTIATED) {
		return SSL_TLSEXT_ERR_OK;
	}

	return SSL_TLSEXT_ERR_ALERT_FATAL;
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

	SecureData* data = malloc(sizeof(SecureData));

	if(!data) {
		// TODO(Totto): better report error
		return NULL;
	}

	SSL_CTX* ssl_context = SSL_CTX_new(TLS_server_method());
	if(ssl_context == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_new failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		free(data);
		return NULL;
	}

	/*
	 * TLS versions older than TLS 1.2 are deprecated by IETF and SHOULD
	 * be avoided if possible.
	 */
	int result = SSL_CTX_set_min_proto_version(ssl_context, TLS1_2_VERSION);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_set_min_proto_version failed:\n");
		ERR_print_errors_cb(error_logger, NULL);

		SSL_CTX_free(ssl_context);
		free(data);
		return 0;
	}

	{ // some options

		/*
		 * Tolerate clients hanging up without a TLS "shutdown".  Appropriate in all
		 * application protocols which perform their own message "framing", and
		 * don't rely on TLS to defend against "truncation" attacks. Like In our case with HTTP
		 */
		uint64_t opts = SSL_OP_IGNORE_UNEXPECTED_EOF;

		// NOTE: this might introduce some error in the login in
		// close_connection_descriptor_advanced
		// TODO: check if that is true

		/*
		 * Block potential CPU-exhaustion attacks by clients that request frequent
		 * renegotiation.
		 */
		opts |= SSL_OP_NO_RENEGOTIATION;

		// makes the server preference have priority over the ones from the client, as the clinet
		// might choose to use unsecure ciphers, over good ones
		opts |= SSL_OP_CIPHER_SERVER_PREFERENCE;

		// set by default in new openssl versions: SSL_OP_NO_COMPRESSION

		uint64_t _new_opts = SSL_CTX_set_options(ssl_context, opts);
		UNUSED(_new_opts);
	}

	result = SSL_CTX_use_certificate_file(ssl_context, public_cert_file, SSL_FILETYPE_PEM);
	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_use_certificate_file failed:\n");
		ERR_print_errors_cb(error_logger, NULL);

		SSL_CTX_free(ssl_context);
		free(data);
		return NULL;
	}

	result = SSL_CTX_use_PrivateKey_file(ssl_context, private_cert_file, SSL_FILETYPE_PEM);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_use_PrivateKey_file failed:\n");
		ERR_print_errors_cb(error_logger, NULL);

		SSL_CTX_free(ssl_context);
		free(data);
		return NULL;
	}

	result = SSL_CTX_check_private_key(ssl_context);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_check_private_key failed:\n");
		ERR_print_errors_cb(error_logger, NULL);

		SSL_CTX_free(ssl_context);
		free(data);
		return NULL;
	}

	// SSL_VERIFY_NONE => Server mode : the server will not send a client certificate request to the
	// client,
	//       so the client will not send a certificate.

	// don't setup storage for verification of client certificates, as http client certificates are
	// not verified (if they are even really used, outside of the part, where they are needed to
	// establish a handshake?)
	SSL_CTX_set_verify(ssl_context, SSL_VERIFY_NONE, NULL);

	// setup ALPN for http/1.1 and http/2

	result = SSL_CTX_set_alpn_protos(ssl_context, g_alpn_protos, sizeof(g_alpn_protos));

	if(result != 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_CTX_set_alpn_protos failed:\n");
		ERR_print_errors_cb(error_logger, NULL);

		SSL_CTX_free(ssl_context);
		free(data);
		return NULL;
	}

	SSL_CTX_set_alpn_select_cb(ssl_context, alpn_select_cb, NULL);

	data->ssl_context = ssl_context;

	return data;
}

static void free_secure_data(SecureData* data) {

	SSL_CTX_free(data->ssl_context);
	free(data);
}

#endif

SecureOptions* initialize_secure_options(bool secure, const char* const public_cert_file,
                                         const char* const private_cert_file) {

	SecureOptions* options = malloc(sizeof(SecureOptions));

	if(!options) {
		// TODO(Totto): better report error
		return NULL;
	}

	if(!secure) {
		options->type = SecureOptionsTypeNotSecure;
		return options;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNUSED(public_cert_file);
	UNUSED(private_cert_file);
	UNREACHABLE();
#else

	options->type = SecureOptionsTypeSecure;

	SecureData* data = initialize_secure_data(public_cert_file, private_cert_file);

	if(data == NULL) {
		free(options);
		return NULL;
	}

	options->data.data = data;

	return options;
#endif
}

void free_secure_options(SecureOptions* const options) {

	if(!is_secure(options)) {
		free(options);
		return;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNREACHABLE();
#else

	free_secure_data(options->data.data);
	free(options);
#endif
}

bool is_secure_context(const ConnectionContext* const context) {
	return context->type == // NOLINT(readability-implicit-bool-conversion)
	       SecureOptionsTypeSecure;
}

#ifndef _SIMPLE_SERVER_SECURE_DISABLED

static SSL* new_ssl_structure_from_ctx(SSL_CTX* ssl_context) {
	SSL* ssl_structure = SSL_new(ssl_context);
	if(ssl_structure == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_new failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return NULL;
	}

	return ssl_structure;
}

#endif

ConnectionContext* get_connection_context(const SecureOptions* const options) {

	ConnectionContext* context = malloc(sizeof(ConnectionContext));

	if(!context) {
		// TODO(Totto): better report error
		return NULL;
	}

	if(!is_secure(options)) {
		context->type = SecureOptionsTypeNotSecure;
		return context;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNUSED(context);
	UNREACHABLE();
#else

	context->type = SecureOptionsTypeSecure;

	SecureData* data = options->data.data;

	SSL* ssl_structure = new_ssl_structure_from_ctx(data->ssl_context);

	if(ssl_structure == NULL) {
		free(context);
		return NULL;
	}

	context->data.data.ssl_structure = ssl_structure;
	context->data.data.options = options;

	return context;
#endif
}

ConnectionContext* copy_connection_context(const ConnectionContext* const old_context) {

	ConnectionContext* context = malloc(sizeof(ConnectionContext));

	if(!context) {
		// TODO(Totto): better report error
		return NULL;
	}

	if(!is_secure_context(old_context)) {
		context->type = SecureOptionsTypeNotSecure;
		return context;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNUSED(context);
	UNREACHABLE();
#else

	context->type = SecureOptionsTypeSecure;

	const SecureOptions* const options = old_context->data.data.options;

	SecureData* data = options->data.data;

	SSL* ssl_structure = new_ssl_structure_from_ctx(data->ssl_context);

	if(ssl_structure == NULL) {
		free(context);
		return NULL;
	}

	context->data.data.ssl_structure = ssl_structure;
	context->data.data.options = options;

	return context;
#endif
}

void free_connection_context(ConnectionContext* context) {

	if(!is_secure_context(context)) {
		free(context);
		return;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNREACHABLE();
#else

	SSL_free(context->data.data.ssl_structure);

	free(context);
#endif
}

static bool is_secure_descriptor(const ConnectionDescriptor* const descriptor) {
	return descriptor->type == // NOLINT(readability-implicit-bool-conversion)
	       SecureOptionsTypeSecure;
}

ConnectionDescriptor* get_connection_descriptor(const ConnectionContext* const context,
                                                int native_fd) {

	ConnectionDescriptor* descriptor = malloc(sizeof(ConnectionDescriptor));

	if(!descriptor) {
		// TODO(Totto): better report error
		return NULL;
	}

	if(!is_secure_context(context)) {
		descriptor->type = SecureOptionsTypeNotSecure;
		descriptor->data.normal = (NormalConnectionData){ .fd = native_fd };
		return descriptor;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNUSED(context);
	UNREACHABLE();
#else

	descriptor->type = SecureOptionsTypeSecure;

	SSL* ssl_structure = context->data.data.ssl_structure;

	int result = SSL_set_fd(ssl_structure, native_fd);

	if(result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_set_fd failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		free(descriptor);
		return NULL;
	}

	int ssl_result = SSL_accept(ssl_structure);

	if(ssl_result != 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_accept failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		free(descriptor);
		return NULL;
	}

	SecureConnectionData secure_data = {
		.ssl_structure = ssl_structure,
		.protocol = ProtocolSelectedNone,
	};

	{ // get selected protocol

		const unsigned char* proto = NULL;
		unsigned int proto_len = 0;

		SSL_get0_alpn_selected(ssl_structure, &proto, &proto_len);

		if(proto_len != 0) {

			if(proto_len == 2 && memcmp(proto, ALPN_PROTO_H2, H2_PROTO_SIZE) == 0) {
				// HTTP/2 selected
				secure_data.protocol = ProtocolSelectedHttp2;
			} else if(proto_len == 8 && memcmp(proto, ALPN_PROTO_H1_1, H1_1_PROTO_SIZE) == 0) {
				// HTTP/1.1 selected
				secure_data.protocol = ProtocolSelectedHttp1Dot1;
			}
		}
	}

	descriptor->data.secure = secure_data;

	return descriptor;
#endif
}

int close_connection_descriptor(ConnectionDescriptor* descriptor) {

	return close_connection_descriptor_advanced(descriptor, NULL, false);
}

int close_connection_descriptor_advanced(ConnectionDescriptor* descriptor,
                                         ConnectionContext* const context, bool allow_reuse) {

	if(!is_secure_descriptor(descriptor)) {
		int result = close(descriptor->data.normal.fd);
		free(descriptor);
		return result;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNUSED(context);
	UNUSED(allow_reuse);
	UNREACHABLE();
#else

	SSL* ssl_structure = descriptor->data.secure.ssl_structure;

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

			LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_shutdown failed:\n");
			ERR_print_errors_cb(error_logger, NULL);
			errno = ESSL;
			return -1;
		}

	} while(result == 0);

	// check the way the connection was closed, either both ends closed it correctly, or the other
	// side didn't clean up correctly
	int shutdown_flags = SSL_get_shutdown(ssl_structure);
	bool was_closed_correctly = false;

	if((shutdown_flags & SSL_SENT_SHUTDOWN) != 0 && (shutdown_flags & SSL_RECEIVED_SHUTDOWN) != 0) {
		was_closed_correctly = true;
	}

	// if it was closed correctly, we can reuse the connection, otherwise we can't
	if(was_closed_correctly && allow_reuse) { // NOLINT(readability-implicit-bool-conversion)

		/* SSL_clear() resets the SSL object to allow for another connection. The reset operation
		 * however keeps several settings of the last sessions (some of these settings were made
		 * automatically during the last handshake). It only makes sense when opening a new session
		 * (or reusing an old one) with the same peer that shares these settings. SSL_clear() is not
		 * a short form for the sequence ssl_free(3); ssl_new(3); .  */
		result = SSL_clear(ssl_structure);

		if(result != 1) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_clear failed:\n");
			ERR_print_errors_cb(error_logger, NULL);
			errno = ESSL;
			return -1;
		}

	} else {
		SSL_free(ssl_structure);

		// if context is NULL; we don't allow reallocating of the new context
		if(context != NULL) {
			assert(context->data.data.ssl_structure == ssl_structure);
			context->data.data.ssl_structure =
			    new_ssl_structure_from_ctx(context->data.data.options->data.data->ssl_context);

			if(context->data.data.ssl_structure == NULL) {
				errno = ESSL;
				return -1;
			}
		}
	}

	free(descriptor);

	return 0;
#endif
}

NODISCARD ReadResult read_from_descriptor(const ConnectionDescriptor* const descriptor,
                                          void* buffer, size_t n_bytes) {
	if(!is_secure_descriptor(descriptor)) {
		ssize_t result = read(descriptor->data.normal.fd, buffer, n_bytes);

		if(result > 0) {
			return (ReadResult){
				.type = ReadResultTypeSuccess,
				.data = { .bytes_read = (size_t)result },
			};
		}

		if(result == 0) {
			return (ReadResult){ .type = ReadResultTypeEOF };
		};

		return (ReadResult){
			.type = ReadResultTypeError,
			.data = { .opaque_error = (OpaqueError){ .errno_error = errno } },
		};
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED

	UNREACHABLE();
#else

	SSL* ssl_structure = descriptor->data.secure.ssl_structure;

	size_t bytes_read = 0;

	int result = SSL_read_ex(ssl_structure, buffer, (int)n_bytes, &bytes_read);

	if(result > 0) {
		return (ReadResult){
			.type = ReadResultTypeSuccess,
			.data = { .bytes_read = bytes_read },
		};
	}

	int error = SSL_get_error(ssl_structure, result);

	unsigned long ssl_error = 0;

	switch(error) {
		case SSL_ERROR_ZERO_RETURN: {
			return (ReadResult){ .type = ReadResultTypeEOF };
		}
		case SSL_ERROR_NONE: {
			ssl_error = 0;
			break;
		}
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
		default: {
			// TODO: what is the best solution here? the err functions in openssl are powerfull,
			// butz depeden on global (thread local?) state, and so they are not really safe to use
			// after this call returns, so we need to store the errors somehow...
			ssl_error = ERR_get_error();
			break;
		}
	}

	return (ReadResult){
		.type = ReadResultTypeError,
		.data = { .opaque_error = (OpaqueError){ .ssl_error = ssl_error } },
	};

#endif
}

NODISCARD char* get_read_error_meaning(const ConnectionDescriptor* descriptor,
                                       OpaqueError opaque_error) {

	if(!is_secure_descriptor(descriptor)) {
		// note for thread safe usage we should use strerror_r, but it doesn't matter that much, if
		// errors occur while error handling, it is not really necessary to handle error messges
		// without errors xD
		return strerror(opaque_error.errno_error);
	}

	// same reason as above, we should use ERR_error_string_n
	return ERR_error_string(opaque_error.ssl_error, NULL);
}

ssize_t write_to_descriptor(const ConnectionDescriptor* const descriptor, void* buffer,
                            size_t n_bytes) {

	if(!is_secure_descriptor(descriptor)) {
		return write(descriptor->data.normal.fd, buffer, n_bytes);
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED

	UNREACHABLE();
#else

	SSL* ssl_structure = descriptor->data.secure.ssl_structure;

	return SSL_write(ssl_structure, buffer, (int)n_bytes);
#endif
}

int get_underlying_socket(const ConnectionDescriptor* const descriptor) {
	if(!is_secure_descriptor(descriptor)) {
		return descriptor->data.normal.fd;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNREACHABLE();
#else

	SSL* ssl_structure = descriptor->data.secure.ssl_structure;

	int ssl_fd = SSL_get_fd(ssl_structure);

	if(ssl_fd < 0) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "SSL_get_fd failed:\n");
		ERR_print_errors_cb(error_logger, NULL);
		return -1;
	}

	return ssl_fd;
#endif
}

NODISCARD ProtocolSelected get_selected_protocol(const ConnectionDescriptor* descriptor) {
	if(!is_secure_descriptor(descriptor)) {
		// no way of negotiating a a protocol before starting reading, so always none
		return ProtocolSelectedNone;
	}

#ifdef _SIMPLE_SERVER_SECURE_DISABLED
	UNREACHABLE();
#else

	return descriptor->data.secure.protocol;

#endif
}
