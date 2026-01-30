#pragma once

#define HTTP_HEADER_NAME(name) g_http_header_##name

#ifdef HTTP_HEADER_IMPL
#define HTTP_HEADER_DEF(name, content) const char* const HTTP_HEADER_NAME(name) = content
#else
#define HTTP_HEADER_DEF(name, content) extern const char* const HTTP_HEADER_NAME(name)
#endif

HTTP_HEADER_DEF(authorization, "authorization");

HTTP_HEADER_DEF(www_authenticate, "www-authenticate");

HTTP_HEADER_DEF(upgrade, "upgrade");

HTTP_HEADER_DEF(connection, "connection");

HTTP_HEADER_DEF(host, "host");

HTTP_HEADER_DEF(origin, "origin");

HTTP_HEADER_DEF(accept_encoding, "accept-encoding");

HTTP_HEADER_DEF(content_type, "content-type");

HTTP_HEADER_DEF(content_length, "content-length");

HTTP_HEADER_DEF(server, "server");

HTTP_HEADER_DEF(content_encoding, "content-encoding");

HTTP_HEADER_DEF(allow, "allow");

// ws specific stuff

HTTP_HEADER_DEF(ws_sec_websocket_key, "sec-websocket-key");

HTTP_HEADER_DEF(ws_sec_websocket_version, "sec-websocket-version");

HTTP_HEADER_DEF(ws_sec_websocket_extensions, "sec-websocket-extensions");

HTTP_HEADER_DEF(ws_sec_websocket_accept, "sec-websocket-accept");
