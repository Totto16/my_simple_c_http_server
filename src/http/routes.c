
#include "./routes.h"
#include "./http_protocol.h"
#include "http/send.h"

struct RouteManagerImpl {
	HTTPRoutes routes;
	const AuthenticationProviders* auth_providers;
};

static HTTPResponseToSend index_executor_fn_extended(SendSettings send_settings,
                                                     const HttpRequest* const http_request,
                                                     const ConnectionContext* const context,
                                                     ParsedURLPath path) {

	UNUSED(path);

	StringBuilder* html_string_builder =
	    http_request_to_html(http_request, is_secure_context(context), send_settings);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body =
		                              http_response_body_from_string_builder(&html_string_builder),
		                          .mime_type = MIME_TYPE_HTML,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend json_executor_fn_extended(SendSettings send_settings,
                                                    const HttpRequest* const http_request,
                                                    const ConnectionContext* const context,
                                                    ParsedURLPath path) {

	UNUSED(path);

	StringBuilder* json_string_builder =
	    http_request_to_json(http_request, is_secure_context(context), send_settings);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body =
		                              http_response_body_from_string_builder(&json_string_builder),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend static_executor_fn(ParsedURLPath path) {

	UNUSED(path);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body =
		                              http_response_body_from_static_string("{\"static\":true}"),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
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

static HTTPResponseToSend huge_executor_fn(ParsedURLPath path) {

	ParsedSearchPathEntry* pretty_key = find_search_key(path.search_path, "pretty");

	bool pretty = pretty_key != NULL;

	StringBuilder* string_builder = get_random_json_string_builder(pretty);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&string_builder),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend auth_executor_fn(ParsedURLPath path, AuthUserWithContext user) {

	UNUSED(path);

	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, "{\"username\": \"");
	string_builder_append_single(string_builder, user.user.username);
	string_builder_append_single(string_builder, "\", \"role\": \"");
	string_builder_append_single(string_builder, user.user.role);
	string_builder_append_single(string_builder, "\", \"provider\": \"");
	string_builder_append_single(string_builder,
	                             get_name_for_auth_provider_type(user.provider_type));
	string_builder_append_single(string_builder, "\"}");

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&string_builder),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

HTTPRoutes get_default_routes(void) {

	HTTPRoutes routes = STBDS_ARRAY_EMPTY;

	{
		// shutdown

		HTTPRoute shutdown = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/shutdown",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeSpecial,
			        .data = { .special = { .type = HTTPRouteSpecialDataTypeShutdown } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, shutdown);
	}

	{
		// index (/)

		HTTPRoute index = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .data = { .normal =
			                      (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutorExtended,
			                                     .fn = { .executor_extended =
			                                                 index_executor_fn_extended } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, index);
	}

	{
		// ws

		HTTPRoute ws_route = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/ws",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeSpecial,
			        .data = { .special = { .type = HTTPRouteSpecialDataTypeWs,
			                               .data = { .ws = { .fragmented = false } } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, ws_route);
	}

	{
		// ws fragmented

		HTTPRoute ws_fragmented = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/ws/fragmented",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeSpecial,
			        .data = { .special = { .type = HTTPRouteSpecialDataTypeWs,
			                               .data = { .ws = { .fragmented = true } } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, ws_fragmented);
	}

	{
		// json

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/json",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .data = { .normal =
			                      (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutorExtended,
			                                     .fn = { .executor_extended =
			                                                 json_executor_fn_extended } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, json);
	}

	{
		// static

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/static",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .data = { .normal =
			                      (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutor,
			                                     .fn = { .executor = static_executor_fn } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, json);
	}

	{
		// huge

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/huge",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .data = { .normal = (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutor,
			                                           .fn = { .executor = huge_executor_fn } } } },
			.auth = { .type = HTTPAuthorizationTypeNone }
		};

		stbds_arrput(routes, json);
	}

	{
		// authenticated

		HTTPRoute json = {
			.method = HTTPRequestRouteMethodGet,
			.path = "/auth",
			.data =
			    (HTTPRouteData){
			        .type = HTTPRouteTypeNormal,
			        .data = { .normal =
			                      (HTTPRouteFn){ .type = HTTPRouteFnTypeExecutorAuth,
			                                     .fn = { .executor_auth = auth_executor_fn } } } },
			.auth = { .type = HTTPAuthorizationTypeSimple }
		};

		stbds_arrput(routes, json);
	}

	return routes;
}

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes routes,
                                                 const AuthenticationProviders* auth_providers) {
	RouteManager* route_manager = malloc(sizeof(RouteManager));

	if(!route_manager) {
		return NULL;
	}

	route_manager->routes = routes;
	route_manager->auth_providers = auth_providers;

	return route_manager;
}

void free_route_manager(RouteManager* route_manager) {

	stbds_arrfree(route_manager->routes);

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

struct SelectedRouteImpl {
	HTTPRouteData route_data;
	ParsedURLPath path;
	AuthUserWithContext* auth_user;
};

NODISCARD static SelectedRoute* selected_route_from_data(HTTPRouteData route_data,
                                                         ParsedURLPath path,
                                                         AuthUserWithContext* auth_user) {
	SelectedRoute* selected_route = malloc(sizeof(SelectedRoute));

	if(!selected_route) {
		return NULL;
	}

	selected_route->route_data = route_data;
	selected_route->path = path;
	selected_route->auth_user = auth_user;

	return selected_route;
}

void free_selected_route(SelectedRoute* selected_route) {
	if(selected_route->auth_user) {
		free(selected_route->auth_user);
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
	HttpAuthHeaderValueType type;
	union {
		struct {
			char* username;
			char* password;
		} basic;
		struct {
			const char* error_message;
		} error;

	} data;
} HttpAuthHeaderValue;

// see: https://developer.mozilla.org/de/docs/Web/HTTP/Reference/Headers/Authorization
NODISCARD static HttpAuthHeaderValue parse_authorization_value(char* value) {

	if(strlen(value) == 0) {
		return (
		    HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
			                      .data = { .error = { .error_message = "empty header value" } } };
	}

	// Syntax:  Authorization: <auth-scheme> <authorization-parameters>))
	char* auth_scheme = strchr(value, ' ');

	if(auth_scheme == NULL) {
		return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
			                          .data = { .error = { .error_message =
			                                                   "no auth-scheme specified" } } };
	}

	*auth_scheme = '\0';

	// TODO(Totto): support more auth-schemes

	// see: https://www.iana.org/assignments/http-authschemes/http-authschemes.xhtml
	if(strcasecmp(auth_scheme, "Basic")) {
		// see https://datatracker.ietf.org/doc/html/rfc7617

		char* value = auth_scheme + 1;

		if(strlen(value) == 0) {
			return (
			    HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
				                      .data = { .error = { .error_message = "data was empty" } } };
		}

		SizedBuffer decoded =
		    base64_decode_buffer((SizedBuffer){ .data = value, .size = strlen(value) });

		if(!decoded.data) {
			return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
				                          .data = { .error = { .error_message =
				                                                   "base64 decoding failed" } } };
		}

		// TODO(Totto): support utf8 here, for the user and the username, but tha needs to be
		// propagated into many places, so not doing it now

		char* decoded_data = malloc(decoded.size + 1);
		memcpy(decoded_data, decoded.data, decoded.size);
		decoded_data[decoded.size] = '\0';

		free_sized_buffer(decoded);

		char* username = decoded_data;
		char* password = NULL;

		char* seperator_idx = strchr(decoded_data, ':');

		if(seperator_idx != NULL) {
			*seperator_idx = '\0';

			password = seperator_idx + 1;
		}

		return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeBasic,
			                          .data = { .basic = { .username = username,
			                                               .password = password } } };
	}

	return (HttpAuthHeaderValue){ .type = HttpAuthHeaderValueTypeError,
		                          .data = { .error = { .error_message =
		                                                   "invalid auth-scheme specified" } } };
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

NODISCARD HttpAuthStatus handle_http_authorization_impl(
    const AuthenticationProviders* auth_providers, const HttpRequest* const request,
    HTTPAuthorizationComplicatedData* data) {

	const HttpHeaderField* authorization_field =
	    find_header_by_key(request->head.header_fields, "Authorization");

	if(authorization_field == NULL) {
		return (HttpAuthStatus){ .type = HttpAuthStatusTypeUnauthorized,
			                     .data = { .unauthorized = {
			                                   .reason = "Authorization header missing" } } };
	}

	char* value_dup = strdup(authorization_field->value);

	HttpAuthHeaderValue result = parse_authorization_value(value_dup);

	if(result.type == HttpAuthHeaderValueTypeError) {
		LOG_MESSAGE(LogLevelError, "Error in parsing the authorization header: %s\n",
		            result.data.error.error_message);

		free(value_dup);
		return (HttpAuthStatus){ .type = HttpAuthStatusTypeError,
			                     .data = { .error = { .error_message = "Header parse error" } } };
	}

	char* username = NULL;
	char* password = NULL;

	switch(result.type) {
		case HttpAuthHeaderValueTypeBasic: {
			username = strdup(result.data.basic.username);
			password = result.data.basic.password == NULL ? NULL : strdup(result.data.basic.password);
			free(result.data.basic.username);
			break;
		}
		case HttpAuthHeaderValueTypeError:
		default: {
			return (
			    HttpAuthStatus){ .type = HttpAuthStatusTypeError,
				                 .data = { .error = { .error_message = "Implemetation error" } } };
		}
	}

	free(value_dup);

	AuthenticationFindResult find_result =
	    authentication_providers_find_user_with_password(auth_providers, username, password);

	free(username);
	free(password);

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
				                 .data = { .error = { .error_message = "Implemetation error" } } };
		}
	}

	// TODO(Totto): handle the data, if presenet, check if the user really is authorized
	UNUSED(data);
}

NODISCARD HttpAuthStatus handle_http_authorization(const AuthenticationProviders* auth_providers,
                                                   const HttpRequest* const request,
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

#define AUTH_REALM "Authentification"

// NOTE: auth usage example: curl "http://test1:test2@localhost:8080/auth" ->
// {"header":"Authorization", "key":"Basic dGVzdDE6dGVzdDI="}

NODISCARD static SelectedRoute* process_matched_route(const RouteManager* const route_manager,
                                                      const HttpRequest* const request,
                                                      HTTPRoute route) {

	AuthUserWithContext* auth_user = NULL;

	if(route.auth.type != HTTPAuthorizationTypeNone) {
		HttpAuthStatus auth_status =
		    handle_http_authorization(route_manager->auth_providers, request, route.auth);

		switch(auth_status.type) {
			case HttpAuthStatusTypeUnauthorized: {

				HttpHeaderFields additional_headers = STBDS_ARRAY_EMPTY;

				char* www_authenticate_buffer = NULL;
				// all 401 have to have a WWW-Authenticate filed according to spec
				FORMAT_STRING(
				    &www_authenticate_buffer,
				    {
					    stbds_arrfree(additional_headers);
					    return NULL;
				    },
				    "WWW-Authenticate%cBasic realm=\"%s\", charset=\"UTF-8\"", '\0', AUTH_REALM);

				add_http_header_field_by_double_str(&additional_headers, www_authenticate_buffer);

				HTTPResponseToSend to_send = { .status = HttpStatusUnauthorized,
					                           .body = http_response_body_empty(),
					                           .mime_type = MIME_TYPE_TEXT,
					                           .additional_headers = additional_headers };

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .data = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, request->head.request_line.path,
				                                auth_user);
			}
			case HttpAuthStatusTypeAuthorized: {
				auth_user = malloc(sizeof(AuthUserWithContext));

				if(!auth_user) {
					HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
						                           .body = http_response_body_from_static_string(
						                               "Internal error: OOM"),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers = STBDS_ARRAY_EMPTY };

					HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
						                         .data = { .internal = { .send = to_send } } };
					return selected_route_from_data(route_data, request->head.request_line.path,
					                                auth_user);
				}

				auth_user->user = auth_status.data.authorized.user;
				auth_user->provider_type = auth_status.data.authorized.provider_type;
				break;
			}
			case HttpAuthStatusTypeAuthorizationError: {
				LOG_MESSAGE(LogLevelError,
				            "An error occured while tyring to process authentication status: %s\n",
				            auth_status.data.auth_error.error);

				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal implementation error in authorization process, type 0"),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = STBDS_ARRAY_EMPTY
				};

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .data = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, request->head.request_line.path,
				                                auth_user);
			}
			case HttpAuthStatusTypeError: {
				LOG_MESSAGE(LogLevelError,
				            "An error occured while tyring to process authentication status: %s\n",
				            auth_status.data.error.error_message);

				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal implementation error in authorization process, type 1"),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = STBDS_ARRAY_EMPTY
				};

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .data = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, request->head.request_line.path,
				                                auth_user);
			}
			default: {
				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal implementation error in authorization process, type 2"),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = STBDS_ARRAY_EMPTY
				};

				HTTPRouteData route_data = { .type = HTTPRouteTypeInternal,
					                         .data = { .internal = { .send = to_send } } };
				return selected_route_from_data(route_data, request->head.request_line.path,
				                                auth_user);
			}
		}
	}

	return selected_route_from_data(route.data, request->head.request_line.path, auth_user);
}

NODISCARD SelectedRoute*
route_manager_get_route_for_request(const RouteManager* const route_manager,
                                    const HttpRequest* const request) {

	for(size_t i = 0; i < stbds_arrlenu(route_manager->routes); ++i) {
		HTTPRoute route = route_manager->routes[i];

		if(is_matching(route.method, request->head.request_line.method)) {

			if(route.path == NULL) {
				return process_matched_route(route_manager, request, route);
			}

			if(strcmp(route.path, request->head.request_line.path.path) == 0) {
				return process_matched_route(route_manager, request, route);
			}
		}
	}

	return NULL;
}

NODISCARD HTTPSelectedRoute get_selected_route_data(const SelectedRoute* const route) {

	return (HTTPSelectedRoute){ .data = route->route_data,
		                        .path = route->path,
		                        .auth_user = route->auth_user };
}

static void free_auth_user(AuthUserWithContext* user) {
	free(user->user.username);
	free(user->user.role);
	free(user);
}

NODISCARD
int route_manager_execute_route(HTTPRouteFn route, const ConnectionDescriptor* const descriptor,
                                SendSettings send_settings, const HttpRequest* const http_request,
                                const ConnectionContext* const context, ParsedURLPath path,
                                AuthUserWithContext* auth_user) {

	HTTPResponseToSend response = {};

	switch(route.type) {
		case HTTPRouteFnTypeExecutor: {
			response = route.fn.executor(path);

			break;
		}
		case HTTPRouteFnTypeExecutorAuth: {
			if(auth_user == NULL) {
				response = (HTTPResponseToSend){
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal error: Authentication required by routem but none given"),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = STBDS_ARRAY_EMPTY
				};
				break;
			}

			response = route.fn.executor_auth(path, *auth_user);

			break;
		}
		case HTTPRouteFnTypeExecutorExtended: {
			response = route.fn.executor_extended(send_settings, http_request, context, path);
			break;
		}
		default: {
			return -11; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			break;
		}
	}

	if(auth_user != NULL) {
		free_auth_user(auth_user);
	}

	int result = send_http_message_to_connection_advanced(descriptor, response, send_settings,
	                                                      http_request->head);

	return result;
}
