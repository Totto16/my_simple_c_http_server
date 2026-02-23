
#include "./routes.h"
#include "./common_log.h"
#include "./debug.h"
#include "./header.h"
#include "./protocol.h"
#include "http/mime.h"
#include "http/send.h"
#include "utils/path.h"

TVEC_IMPLEMENT_VEC_TYPE(HTTPRoute)

TVEC_IMPLEMENT_VEC_TYPE(HTTPFreeFn)

TVEC_IMPLEMENT_VEC_TYPE(HTTPRequestProxy)

struct RouteManagerImpl {
	HTTPRoutes* routes;
	const AuthenticationProviders* auth_providers;
};

static HTTPResponseToSend index_executor_fn_extended(SendSettings send_settings,
                                                     const HttpRequest http_request,
                                                     const ConnectionContext* const context,
                                                     ParsedURLPath path, void* /* data */) {

	UNUSED(path);

	const bool send_body = http_request.head.request_line.method != HTTPRequestMethodHead;

	StringBuilder* html_string_builder =
	    http_request_to_html(http_request, is_secure_context(context), send_settings);

	if(html_string_builder == NULL) {

		HTTPResponseToSend result = { .status = HttpStatusInternalServerError,
			                          .body = http_response_body_from_static_string(
			                              "Internal Server Error  6", send_body),
			                          .mime_type = MIME_TYPE_TEXT,
			                          .additional_headers = TVEC_EMPTY(HttpHeaderField) };

		return result;
	}

	HTTPResponseToSend result = {
		.status = HttpStatusOk,
		.body = http_response_body_from_string_builder(&html_string_builder, send_body),
		.mime_type = MIME_TYPE_HTML,
		.additional_headers = TVEC_EMPTY(HttpHeaderField),
	};

	return result;
}

static HTTPResponseToSend
well_known_folder_fn_extended(SendSettings /* send_settings */, const HttpRequest http_request,
                              const ConnectionContext* const /* context */, ParsedURLPath path,
                              void* data) {

	LogCollector* collector = (LogCollector*)data;

	const bool send_body = http_request.head.request_line.method != HTTPRequestMethodHead;

	if(strcmp(path.path, "/.well-known/access.log") == 0) {

		StringBuilder* builder = log_collector_to_string_builder(collector);

		if(builder == NULL) {

			HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
				                           .body = http_response_body_from_static_string(
				                               "Internal Server Error: 71", send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

			return to_send;
		}

		HTTPResponseToSend to_send = { .status = HttpStatusOk,
			                           .body = http_response_body_from_string_builder(&builder,
			                                                                          send_body),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

		return to_send;
	}

	HTTPResponseToSend to_send = { .status = HttpStatusNotFound,
		                           .body = http_response_body_from_static_string(
		                               "Well Known entry not found", send_body),
		                           .mime_type = MIME_TYPE_TEXT,
		                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

	return to_send;
}

static void log_collector_collect_fn(const HttpRequest http_request,
                                     const HTTPResponseToSend response, IPAddress address,
                                     void* data) {
	LogCollector* collector = (LogCollector*)data;

	log_collector_collect(collector, address, http_request, response);
}

static HTTPResponseToSend json_executor_fn_extended(SendSettings send_settings,
                                                    const HttpRequest http_request,
                                                    const ConnectionContext* const context,
                                                    ParsedURLPath /* path */, void* /* data */) {

	const bool send_body = http_request.head.request_line.method != HTTPRequestMethodHead;

	StringBuilder* json_string_builder =
	    http_request_to_json(http_request, is_secure_context(context), send_settings);

	HTTPResponseToSend result = {
		.status = HttpStatusOk,
		.body = http_response_body_from_string_builder(&json_string_builder, send_body),
		.mime_type = MIME_TYPE_JSON,
		.additional_headers = TVEC_EMPTY(HttpHeaderField),
	};
	return result;
}

static HTTPResponseToSend static_executor_fn(ParsedURLPath /* path */, bool send_body) {

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_static_string("{\"static\":true}",
		                                                                        send_body),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = TVEC_EMPTY(HttpHeaderField) };
	return result;
}

static char json_get_random_char(void) {

	char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-*$%.#";
	uint32_t random_index = get_random_byte_in_range(0, (sizeof(charset) / sizeof(*charset)) - 1);

	return charset[random_index];
}

static char* json_get_random_string(void) {
	uint32_t random_key_length = get_random_byte_in_range(
	    6, 30); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	char* key = malloc((size_t)random_key_length + 3);

	key[0] = '"';
	key[random_key_length + 1] = '"';
	key[random_key_length + 2] = '\0';

	for(uint32_t i = 1; i < random_key_length + 1; ++i) {
		key[i] = json_get_random_char();
	}

	return key;
}

static char* json_get_random_boolean(void) {
	uint32_t number = get_random_byte();

	if(number % 2 == 0) {
		return strdup("false");
	}

	return strdup("true");
}

static char* json_get_random_number(void) {

	uint32_t number = get_random_byte();

	StringBuilder* result = string_builder_init();

	STRING_BUILDER_APPENDF(result, return NULL;, "%u", number);

	return string_builder_release_into_string(&result);
}

static char* json_get_null(void) {
	return strdup("null");
}

static char* json_get_random_primitive_value(void) {
	uint32_t random_type = get_random_byte_in_range(
	    0, 4); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	switch(random_type) {
		case 0: return json_get_random_string();
		case 1: return json_get_random_number();
		case 2: return json_get_random_boolean();
		case 3:
		default: return json_get_null();
	}
}

static char* json_get_random_key(void) {
	return json_get_random_string();
}

static void add_random_object_key_and_value(StringBuilder* string_builder, bool pretty) {
	char* key = json_get_random_key();

	string_builder_append_string(string_builder, key);
	if(pretty) {
		string_builder_append_single(string_builder, " ");
	}
	string_builder_append_single(string_builder, ":");

	char* value = json_get_random_primitive_value();

	string_builder_append_string(string_builder, value);
}

static void add_random_json_object(StringBuilder* string_builder, bool pretty) {

	uint32_t random_key_amount = get_random_byte_in_range(
	    4, 20); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	string_builder_append_single(string_builder, "{");
	if(pretty) {
		string_builder_append_single(string_builder, "\n");
	}

	for(size_t i = 0; i < random_key_amount; ++i) {
		add_random_object_key_and_value(string_builder, pretty);

		string_builder_append_single(string_builder, ",");
		if(pretty) {
			string_builder_append_single(string_builder, "\n");
		}
	}

	add_random_object_key_and_value(string_builder, pretty);

	if(pretty) {
		string_builder_append_single(string_builder, "\n");
	}
	string_builder_append_single(string_builder, "}");

	//
}

static StringBuilder* get_random_json_string_builder(bool pretty) {

	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, "[");

	if(pretty) {
		string_builder_append_single(string_builder, "\n");
	}

	// for compression tests, has to be at least  1 MB big, so that it can be tested accordingly
	size_t minimum_size =
	    1 << 20; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	while(string_builder_get_string_size(string_builder) < minimum_size) {
		if(pretty) {
			string_builder_append_single(string_builder, "\n");
		}

		add_random_json_object(string_builder, pretty);
		string_builder_append_single(string_builder, ",");

		if(pretty) {
			string_builder_append_single(string_builder, "\n");
		}
	}

	add_random_json_object(string_builder, pretty);

	if(pretty) {
		string_builder_append_single(string_builder, "\n");
	}

	string_builder_append_single(string_builder, "]");

	return string_builder;
}

static HTTPResponseToSend huge_executor_fn(ParsedURLPath path, const bool send_body) {

	const ParsedSearchPathEntry* pretty_key = find_search_key(path.search_path, "pretty");

	bool pretty = pretty_key != NULL;

	StringBuilder* string_builder = get_random_json_string_builder(pretty);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&string_builder,
		                                                                         send_body),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = TVEC_EMPTY(HttpHeaderField) };
	return result;
}

static HTTPResponseToSend auth_executor_fn(ParsedURLPath /* path */, AuthUserWithContext user,
                                           const bool send_body) {

	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, "{\"username\": \"");
	string_builder_append_single(string_builder, tstr_cstr(&user.user.username));
	string_builder_append_single(string_builder, "\", \"role\": \"");
	string_builder_append_single(string_builder, get_name_for_user_role(user.user.role));
	string_builder_append_single(string_builder, "\", \"provider\": \"");
	string_builder_append_single(string_builder,
	                             get_name_for_auth_provider_type(user.provider_type));
	string_builder_append_single(string_builder, "\"}");

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&string_builder,
		                                                                         send_body),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = TVEC_EMPTY(HttpHeaderField) };
	return result;
}

HTTPRoutes* get_default_routes(void) {

	HTTPRoutes* routes = malloc(sizeof(HTTPRoutes));

	if(routes == NULL) {
		return NULL;
	}

	*routes = (HTTPRoutes){
		.routes = TVEC_EMPTY(HTTPRoute),
		.free_fns = TVEC_EMPTY(HTTPFreeFn),
		.proxies = TVEC_EMPTY(HTTPRequestProxy),
	};

	{
		// shutdown

		HTTPRoute shutdown = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/shutdown",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeSpecial,
			        .value = { .special = { .type = HTTPRouteSpecialDataTypeShutdown } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, shutdown);
		UNUSED(_);
	}

	{
		// index (/)

		HTTPRoute index = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .value = { .normal =
			                       (HTTPRouteFn){
			                           .type = HTTPRouteFnTypeExecutorExtended,
			                           .value = { .extended_data = { .executor_extended =
			                                                             index_executor_fn_extended,
			                                                         .data = NULL } },
			                       } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, index);
		UNUSED(_);
	}

	{
		// ws

		HTTPRoute ws_route = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/ws",
			    },
			.data =
			    (HTTPRouteData){ .type = HTTPRouteTypeSpecial,
			                     .value = { .special = { .type = HTTPRouteSpecialDataTypeWs } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, ws_route);
		UNUSED(_);
	}

	{
		// json

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/json",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .value = { .normal =
			                       (HTTPRouteFn){
			                           .type = HTTPRouteFnTypeExecutorExtended,
			                           .value = { .extended_data = { .executor_extended =
			                                                             json_executor_fn_extended,
			                                                         .data = NULL } } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, json);
		UNUSED(_);
	}

	{
		// static

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/static",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .value = { .normal = (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutor,
			                                            .value = { .fn_executor =
			                                                           static_executor_fn } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, json);
		UNUSED(_);
	}

	{
		// huge

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/huge",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .value = { .normal = (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutor,
			                                            .value = { .fn_executor =
			                                                           huge_executor_fn } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, json);
		UNUSED(_);
	}

	{
		// authenticated

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/auth",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .value = { .normal = (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutorAuth,
			                                            .value = { .fn_executor_auth =
			                                                           auth_executor_fn } } } },
			.auth = { .type = HTTPAuthorizationTypeSimple }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, json);
		UNUSED(_);
	}

	return routes;
}

NODISCARD HTTPRoutes* get_webserver_test_routes(void) {
	HTTPRoutes* routes = malloc(sizeof(HTTPRoutes));

	if(routes == NULL) {
		return NULL;
	}

	*routes = (HTTPRoutes){
		.routes = TVEC_EMPTY(HTTPRoute),
		.free_fns = TVEC_EMPTY(HTTPFreeFn),
		.proxies = TVEC_EMPTY(HTTPRequestProxy),
	};

	{
		// shutdown

		HTTPRoute shutdown = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeExact,
			        .data = "/_special/shutdown",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeSpecial,
			        .value = { .special = { .type = HTTPRouteSpecialDataTypeShutdown } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, shutdown);
		UNUSED(_);
	}

	// logs collector

	LogCollector* log_collector = initialize_log_collector();

	if(!log_collector) {
		LOG_MESSAGE(LogLevelWarn, "Failed to initialize log collector: %s\n", "<unknown error>")
	} else {

		HTTPRequestProxy logs_collector_proxy = { .type = HTTPRequestProxyTypePost,
			                                      .value = {
			                                          .post = log_collector_collect_fn,
			                                      },
												.data = log_collector };

		auto _ = TVEC_PUSH(HTTPRequestProxy, &routes->proxies, logs_collector_proxy);
		UNUSED(_);

		HTTPFreeFn free_route = { .data = log_collector, .fn = (FreeFnImpl)free_log_collector };

		auto _1 = TVEC_PUSH(HTTPFreeFn, &routes->free_fns, free_route);
		UNUSED(_1);

		// logs folder
		// in the "/.well-known/" folder
		// see https://www.rfc-editor.org/rfc/rfc8615

		HTTPRoute well_known_folder = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeStartsWith,
			        .data = "/.well-known/",
			    },
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .value = { .normal =
			                       (HTTPRouteFn){
			                           .type = HTTPRouteFnTypeExecutorExtended,
			                           .value = { .extended_data = { .executor_extended =
			                                                             well_known_folder_fn_extended,
			                                                         .data = log_collector } },

			                       } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _2 = TVEC_PUSH(HTTPRoute, &routes->routes, well_known_folder);
		UNUSED(_2);
	}

	// note, as routes get checked in order, this works, even if / gets mapped to the server_folder!

	{
		// / folder serve

		static const char* s_folder_env_variable = "WEBSERVER_TEST_WEBROOT";

		char* folder_path = getenv(s_folder_env_variable);

		if(folder_path == NULL) {
			LOG_MESSAGE(LogLevelError, "Couldn't find required env variable '%s'\n",
			            s_folder_env_variable);
			return NULL;
		}

		char* folder_path_resolved = get_serve_folder(folder_path);

		if(folder_path_resolved == NULL) {
			return NULL;
		}

		HTTPRoute serve_route = {
			.method = HTTPRequestRouteMethodGet,
			.path =
			    (HTTPRoutePath){
			        .type = HTTPRoutePathTypeStartsWith,
			        .data = "/",
			    },
			.data = (HTTPRouteData){ .type = HTTPRouteTypeServeFolder,
			                         .value = { .serve_folder =
			                                        (HTTPRouteServeFolder){
			                                            .type = HTTPRouteServeFolderTypeRelative,
			                                            .folder_path = folder_path_resolved,
			                                        } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		auto _ = TVEC_PUSH(HTTPRoute, &routes->routes, serve_route);
		UNUSED(_);

		HTTPFreeFn free_route = { .data = folder_path_resolved, .fn = free };

		auto _1 = TVEC_PUSH(HTTPFreeFn, &routes->free_fns, free_route);
		UNUSED(_1);
	}

	return routes;
}

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes* routes,
                                                 const AuthenticationProviders* auth_providers) {
	RouteManager* route_manager = malloc(sizeof(RouteManager));

	if(!route_manager) {
		return NULL;
	}

	route_manager->routes = routes;
	route_manager->auth_providers = auth_providers;

	return route_manager;
}

static void free_routes(HTTPRoutes* routes) {

	TVEC_FREE(HTTPRoute, &routes->routes);

	for(size_t i = 0; i < TVEC_LENGTH(HTTPFreeFn, routes->free_fns); ++i) {
		HTTPFreeFn free_fn = TVEC_AT(HTTPFreeFn, routes->free_fns, i);
		free_fn.fn(free_fn.data);
	}

	TVEC_FREE(HTTPFreeFn, &routes->free_fns);

	TVEC_FREE(HTTPRequestProxy, &routes->proxies);

	free(routes);
}

void free_route_manager(RouteManager* route_manager) {

	free_routes(route_manager->routes);

	free(route_manager);
}

NODISCARD static bool is_matching(HTTPRequestRouteMethod route_method, HTTPRequestMethod method) {

	if(route_method == HTTPRequestRouteMethodGet && method == HTTPRequestMethodHead) {
		return true;
	}

	if(route_method == HTTPRequestRouteMethodGet && method == HTTPRequestMethodGet) {
		return true;
	}

	if(route_method == HTTPRequestRouteMethodPost && method == HTTPRequestMethodPost) {
		return true;
	}

	return false;
}

NODISCARD static bool is_route_matching(HTTPRoutePath route_path, const char* const path) {

	if(route_path.data == NULL) {
		return true;
	}

	switch(route_path.type) {
		case HTTPRoutePathTypeExact: {
			return strcmp(route_path.data, path) == 0;
		}
		case HTTPRoutePathTypeStartsWith: {

			size_t route_path_len = strlen(route_path.data);

			size_t path_len = strlen(path);

			if(path_len < route_path_len) {
				return false;
			}

			// TODO: use cwalk!
			return strncmp(route_path.data, path, route_path_len) == 0;
		}
		default: {
			return false;
		}
	}
}

struct SelectedRouteImpl {
	HTTPRouteData route_data;
	ParsedURLPath path;
	const char* original_path;
	AuthUserWithContext* auth_user;
};

NODISCARD static SelectedRoute* selected_route_from_data(HTTPRouteData route_data,
                                                         const char* const original_path,
                                                         ParsedURLPath path,
                                                         AuthUserWithContext* auth_user) {
	SelectedRoute* selected_route = malloc(sizeof(SelectedRoute));

	if(!selected_route) {
		return NULL;
	}

	selected_route->route_data = route_data;
	selected_route->path = path;
	selected_route->original_path = original_path;
	selected_route->auth_user = auth_user;

	return selected_route;
}

static void free_auth_user(AuthUserWithContext* user) {
	tstr_free(&(user->user.username));
	free(user);
}

void free_selected_route(SelectedRoute* selected_route) {
	if(selected_route->auth_user) {
		free_auth_user(selected_route->auth_user);
	}
	free(selected_route);
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HttpAuthHeaderValueTypeBasic = 0,
	HttpAuthHeaderValueTypeError,
} HttpAuthHeaderValueType;

typedef struct {
	tstr username;
	tstr password;
} HttpAuthHeaderBasic;

typedef struct {
	HttpAuthHeaderValueType type;
	union {
		HttpAuthHeaderBasic basic;
		const char* error;
	} data;
} HttpAuthHeaderValue;

static void free_http_auth_header_basic(HttpAuthHeaderBasic* const value) {
	tstr_free(&(value->username));
	tstr_free(&(value->password));
}

static void free_http_auth_header_value(HttpAuthHeaderValue* const value) {
	switch(value->type) {
		case HttpAuthHeaderValueTypeBasic: {
			free_http_auth_header_basic(&(value->data.basic));
			break;
		}
		case HttpAuthHeaderValueTypeError:
		default: {
			break;
		}
	}
}

// see: https://developer.mozilla.org/de/docs/Web/HTTP/Reference/Headers/Authorization
NODISCARD static HttpAuthHeaderValue parse_authorization_value(const tstr_view value) {

	if(value.len == 0) {
		return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
			                          .data = { .error = "empty header value" } };
	}

	// Syntax:  Authorization: <auth-scheme> <authorization-parameters>))
	const tstr_split_result split_res = tstr_split(value, " ");

	if(!split_res.ok) {
		return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
			                          .data = { .error = "no auth-params specified" } };
	}

	const tstr_view auth_scheme = split_res.first;
	const tstr_view auth_param = split_res.second;

	// TODO(Totto): support more auth-schemes

	// see: https://www.iana.org/assignments/http-authschemes/http-authschemes.xhtml
	if(tstr_view_eq_ignore_case(auth_scheme, "Basic")) {
		// see https://datatracker.ietf.org/doc/html/rfc7617

		if(auth_param.len == 0) {
			return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
				                          .data = { .error = "data was empty" } };
		}

		SizedBuffer decoded = base64_decode_buffer(
		    (SizedBuffer){ .data = (void*)auth_param.data, .size = auth_param.len });

		if(!decoded.data) {
			return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
				                          .data = { .error = "base64 decoding failed" } };
		}

		// TODO(Totto): support utf8 here, for the user and the username, but tha needs to be
		// propagated into many places, so not doing it now

		const tstr_view decoded_data = tstr_view_from_buffer(decoded);

		const tstr_split_result split_res2 = tstr_split(decoded_data, ":");

		const tstr_view username = split_res2.ok ? split_res2.first : decoded_data;
		const tstr_view password = split_res2.ok ? split_res2.second : TSTR_EMPTY_VIEW;

		HttpAuthHeaderBasic basic = { .username = tstr_from_view(username),
			                          .password = tstr_from_view(password) };

		free_sized_buffer(decoded);

		return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeBasic,
			                          .data = {
			                              .basic = basic,
			                          } };
	}

	return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
		                          .data = { .error = "invalid auth-scheme specified" } };
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HttpAuthStatusTypeUnauthorized = 0,
	HttpAuthStatusTypeAuthorized,
	HttpAuthStatusTypeAuthorizationError,
	HttpAuthStatusTypeError
} HttpAuthStatusType;

typedef struct {
	HttpAuthStatusType type;
	union {
		AuthUserWithContext authorized;
		struct {
			const char* reason;
		} unauthorized;
		struct {
			const char* error;
		} auth_error;
		struct {
			const char* error_message;
		} error;

	} data;
} HttpAuthStatus;

NODISCARD static HttpAuthStatus
handle_http_authorization_impl(const AuthenticationProviders* auth_providers,
                               const HttpRequest request, HTTPAuthorizationComplicatedData* data) {

	const HttpHeaderField* authorization_field =
	    find_header_by_key(request.head.header_fields, HTTP_HEADER_NAME(authorization));

	if(authorization_field == NULL) {
		return (HttpAuthStatus){ .type = HttpAuthStatusTypeUnauthorized,
			                     .data = { .unauthorized = {
			                                   .reason = "Authorization header missing" } } };
	}

	HttpAuthHeaderValue result =
	    parse_authorization_value(tstr_view_from(authorization_field->value));

	if(result.type == HttpAuthHeaderValueTypeError) {
		LOG_MESSAGE(LogLevelError, "Error in parsing the authorization header: %s\n",
		            result.data.error);

		return (HttpAuthStatus){ .type = HttpAuthStatusTypeError,
			                     .data = { .error = { .error_message = "Header parse error" } } };
	}

	// note: this potential memory leak is not one, as it is just returned and than later freed by
	// the cosnuming functions
	tstr username = tstr_init(); // NOLINT(clang-analyzer-unix.Malloc)
	tstr password = tstr_init();

	switch(result.type) {
		case HttpAuthHeaderValueTypeBasic: {
			const HttpAuthHeaderBasic basic = result.data.basic;
			username = basic.username;
			password = basic.password;
			result.data.basic =
			    (HttpAuthHeaderBasic){ .username = tstr_init(), .password = tstr_init() };
			break;
		}
		case HttpAuthHeaderValueTypeError:
		default: {
			return (
			    HttpAuthStatus){ .type = HttpAuthStatusTypeError,
				                 .data = { .error = { .error_message = "Implementation error" } } };
		}
	}

	free_http_auth_header_value(&result);

	AuthenticationFindResult find_result =
	    authentication_providers_find_user_with_password(auth_providers, &username, &password);

	tstr_free(&username);
	tstr_free(&password);

	switch(find_result.validity) {
		case AuthenticationValidityNoSuchUser: {

			return (HttpAuthStatus){ .type = HttpAuthStatusTypeUnauthorized,
				                     .data = { .unauthorized = { .reason = "no such user" } } };
		}
		case AuthenticationValidityWrongPassword: {

			return (HttpAuthStatus){ .type = HttpAuthStatusTypeUnauthorized,
				                     .data = { .unauthorized = { .reason = "wrong password" } } };
		}
		case AuthenticationValidityOk: {

			return (HttpAuthStatus){ .type = HttpAuthStatusTypeAuthorized,
				                     .data = { .authorized = find_result.data.ok } };
		}
		case AuthenticationValidityError: {
			LOG_MESSAGE(LogLevelError, "Error in account find operation: %s\n",
			            find_result.data.error.error_message);
			return (HttpAuthStatus){
				.type = HttpAuthStatusTypeAuthorizationError,
				.data = { .error = { .error_message =
				                         "Underlying authentication provider error" } }
			};
		}
		default: {
			return (
			    HttpAuthStatus){ .type = HttpAuthStatusTypeError,
				                 .data = { .error = { .error_message = "Implementation error" } } };
		}
	}

	// TODO(Totto): handle the data, if present, check if the user really is authorized
	UNUSED(data);
}

NODISCARD static HttpAuthStatus
handle_http_authorization(const AuthenticationProviders* auth_providers, const HttpRequest request,
                          HTTPAuthorization auth) {

	switch(auth.type) {
		case HTTPAuthorizationTypeNone: {
			return (
			    HttpAuthStatus){ .type = HttpAuthStatusTypeError,
				                 .data = { .error = { .error_message = "Implementation error" } } };
		}
		case HTTPAuthorizationTypeSimple: {
			return handle_http_authorization_impl(auth_providers, request, NULL);
		}
		case HTTPAuthorizationTypeComplicated: {
			return handle_http_authorization_impl(auth_providers, request, &auth.data.complicated);
		}
		default: {
			return (
			    HttpAuthStatus){ .type = HttpAuthStatusTypeError,
				                 .data = { .error = { .error_message = "Implementation error" } } };
		}
	}
}

#define DEFAULT_AUTH_REALM "Authentication"

// NOTE: auth usage example: curl "http://test1:test2@localhost:8080/auth" ->
// {"header":"Authorization", "key":"Basic dGVzdDE6dGVzdDI="}

NODISCARD static SelectedRoute* process_matched_route(const RouteManager* const route_manager,
                                                      HttpRequestProperties http_properties,
                                                      const HttpRequest request, HTTPRoute route) {

	if(http_properties.type != HTTPPropertyTypeNormal) {
		return NULL;
	}

	const ParsedURLPath normal_data = http_properties.data.normal;

	const bool send_body = request.head.request_line.method != HTTPRequestMethodHead;

	AuthUserWithContext* auth_user = NULL;

	if(route.auth.type != HTTPAuthorizationTypeNone) {
		HttpAuthStatus auth_status =
		    handle_http_authorization(route_manager->auth_providers, request, route.auth);

		switch(auth_status.type) {
			case HttpAuthStatusTypeUnauthorized: {

				HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

				char* www_authenticate_buffer = NULL;
				// all 401 have to have a WWW-Authenticate filed according to spec
				FORMAT_STRING(
				    &www_authenticate_buffer,
				    {
					    TVEC_FREE(HttpHeaderField, &additional_headers);
					    return NULL;
				    },
				    "Basic realm=\"%s\", charset=\"UTF-8\"", DEFAULT_AUTH_REALM);

				add_http_header_field_const_key_dynamic_value(&additional_headers,
				                                              HTTP_HEADER_NAME(www_authenticate),
				                                              www_authenticate_buffer);

#ifndef NDEBUG
				add_http_header_field_const_key_const_value(&additional_headers,
				                                            HTTP_HEADER_NAME(x_special_reason),
				                                            auth_status.data.unauthorized.reason);

#endif

				HTTPResponseToSend to_send = { .status = HttpStatusUnauthorized,
					                           .body = http_response_body_empty(),
					                           .mime_type = MIME_TYPE_TEXT,
					                           .additional_headers = additional_headers };

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .value = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, route.path.data, normal_data,
				                                auth_user);
			}
			case HttpAuthStatusTypeAuthorized: {
				auth_user = malloc(sizeof(AuthUserWithContext));

				if(!auth_user) {
					HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
						                           .body = http_response_body_from_static_string(
						                               "Internal error: OOM", send_body),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers =
						                               TVEC_EMPTY(HttpHeaderField) };

					HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
						                         .value = { .internal = { .send = to_send } } };
					return selected_route_from_data(route_data, route.path.data, normal_data,
					                                auth_user);
				}

				auth_user->user = auth_status.data.authorized.user;
				auth_user->provider_type = auth_status.data.authorized.provider_type;
				break;
			}
			case HttpAuthStatusTypeAuthorizationError: {
				LOG_MESSAGE(LogLevelError,
				            "An error occurred while tyring to process authentication status: %s\n",
				            auth_status.data.auth_error.error);

				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal implementation error in authorization process, type 0",
					    send_body),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = TVEC_EMPTY(HttpHeaderField)
				};

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .value = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, route.path.data, normal_data,
				                                auth_user);
			}
			case HttpAuthStatusTypeError: {
				LOG_MESSAGE(LogLevelError,
				            "An error occurred while tyring to process authentication status: %s\n",
				            auth_status.data.error.error_message);

				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal implementation error in authorization process, type 1",
					    send_body),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = TVEC_EMPTY(HttpHeaderField)
				};

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .value = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, route.path.data, normal_data,
				                                auth_user);
			}
			default: {
				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal implementation error in authorization process, type 2",
					    send_body),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = TVEC_EMPTY(HttpHeaderField)
				};

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .value = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, route.path.data, normal_data,
				                                auth_user);
			}
		}
	}

	return selected_route_from_data( // NOLINT(clang-analyzer-unix.Malloc)
	    route.data, route.path.data, normal_data, auth_user);
}

NODISCARD SelectedRoute*
route_manager_get_route_for_request(const RouteManager* const route_manager,
                                    HttpRequestProperties http_properties,
                                    const HttpRequest request, const IPAddress address) {

	if(http_properties.type != HTTPPropertyTypeNormal) {
		return NULL;
	}

	const ParsedURLPath normal_data = http_properties.data.normal;

	for(size_t i = 0; i < TVEC_LENGTH(HTTPRequestProxy, route_manager->routes->proxies); ++i) {
		HTTPRequestProxy proxy = TVEC_AT(HTTPRequestProxy, route_manager->routes->proxies, i);

		if(proxy.type == HTTPRequestProxyTypePre) {
			proxy.value.pre(request, address, proxy.data);
		}
	}

	for(size_t i = 0; i < TVEC_LENGTH(HTTPRoute, route_manager->routes->routes); ++i) {
		HTTPRoute route = TVEC_AT(HTTPRoute, route_manager->routes->routes, i);

		if(is_matching(route.method, request.head.request_line.method)) {

			if(is_route_matching(route.path, normal_data.path)) {
				return process_matched_route(route_manager, http_properties, request, route);
			}
		}
	}

	return NULL;
}

NODISCARD HTTPSelectedRoute get_selected_route_data(const SelectedRoute* const route) {

	return (HTTPSelectedRoute){ .data = route->route_data,
		                        .path = route->path,
		                        .original_path = route->original_path,
		                        .auth_user = route->auth_user };
}

NODISCARD
int route_manager_execute_route(const RouteManager* const route_manager, HTTPRouteFn route,
                                const ConnectionDescriptor* const descriptor,
                                HTTPGeneralContext* general_context, SendSettings send_settings,
                                const HttpRequest http_request,
                                const ConnectionContext* const context, ParsedURLPath path,
                                AuthUserWithContext* auth_user, IPAddress address) {

	HTTPResponseToSend response = {};

	const bool send_body = http_request.head.request_line.method != HTTPRequestMethodHead;

	switch(route.type) {
		case HTTPRouteFnTypeExecutor: {
			response = route.value.fn_executor(path, send_body);

			break;
		}
		case HTTPRouteFnTypeExecutorAuth: {
			if(auth_user == NULL) {
				response = (HTTPResponseToSend){
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal error: Authentication required by route, but none given",
					    send_body),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = TVEC_EMPTY(HttpHeaderField)
				};
				break;
			}

			response = route.value.fn_executor_auth(path, *auth_user, send_body);

			break;
		}
		case HTTPRouteFnTypeExecutorExtended: {
			response = route.value.extended_data.executor_extended(
			    send_settings, http_request, context, path, route.value.extended_data.data);
			break;
		}
		default: {
			return -11; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			break;
		}
	}

	if(http_request.head.request_line.method == HTTPRequestMethodHead) {
		response.body.send_body_data = false;
	}

	for(size_t i = 0; i < TVEC_LENGTH(HTTPRequestProxy, route_manager->routes->proxies); ++i) {
		HTTPRequestProxy proxy = TVEC_AT(HTTPRequestProxy, route_manager->routes->proxies, i);

		if(proxy.type == HTTPRequestProxyTypePost) {
			proxy.value.post(http_request, response, address, proxy.data);
		}
	}

	int result =
	    send_http_message_to_connection(general_context, descriptor, response, send_settings);

	return result;
}
