#pragma once

#include <tstr.h>

#define HTTP_HEADER_NAME(name) g_http_header_##name

#ifdef HTTP_HEADER_IMPL
	#define HTTP_HEADER_MAKE(name, content) const tstr HTTP_HEADER_NAME(name) = TSTR_LIT(content)
#else
	#define HTTP_HEADER_MAKE(name, content) extern const tstr HTTP_HEADER_NAME(name)
#endif

HTTP_HEADER_MAKE(authorization, "authorization");

HTTP_HEADER_MAKE(www_authenticate, "www-authenticate");

HTTP_HEADER_MAKE(upgrade, "upgrade");

HTTP_HEADER_MAKE(connection, "connection");

HTTP_HEADER_MAKE(host, "host");

HTTP_HEADER_MAKE(origin, "origin");

HTTP_HEADER_MAKE(accept_encoding, "accept-encoding");

HTTP_HEADER_MAKE(content_type, "content-type");

HTTP_HEADER_MAKE(content_length, "content-length");

HTTP_HEADER_MAKE(server, "server");

HTTP_HEADER_MAKE(content_encoding, "content-encoding");

HTTP_HEADER_MAKE(allow, "allow");

HTTP_HEADER_MAKE(transfer_encoding, "transfer-encoding");

HTTP_HEADER_MAKE(content_transfer_encoding, "content-transfer-encoding");

HTTP_HEADER_MAKE(content_description, "content-description");

HTTP_HEADER_MAKE(content_disposition, "content-disposition");

HTTP_HEADER_MAKE(date, "date");

HTTP_HEADER_MAKE(http2_settings, "http2-settings");

HTTP_HEADER_MAKE(alt_svc, "alt-svc");

// ws specific stuff

HTTP_HEADER_MAKE(ws_sec_websocket_key, "sec-websocket-key");

HTTP_HEADER_MAKE(ws_sec_websocket_version, "sec-websocket-version");

HTTP_HEADER_MAKE(ws_sec_websocket_extensions, "sec-websocket-extensions");

HTTP_HEADER_MAKE(ws_sec_websocket_accept, "sec-websocket-accept");

// x- special stuff

HTTP_HEADER_MAKE(x_special_reason, "x-special-reason");

HTTP_HEADER_MAKE(x_shutdown, "x-shutdown");

// http2 pseudo header fields

HTTP_HEADER_MAKE(http2_pseudo_method, ":method");

HTTP_HEADER_MAKE(http2_pseudo_scheme, ":scheme");

HTTP_HEADER_MAKE(http2_pseudo_authority, ":authority");

HTTP_HEADER_MAKE(http2_pseudo_path, ":path");

HTTP_HEADER_MAKE(http2_pseudo_status, ":status");
