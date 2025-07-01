
#include "./routes.h"
#include "./http_protocol.h"
#include "http/send.h"

struct RouteManagerImpl {
	HTTPRoutes routes;
};

static HTTPResponseToSend index_executor_fn_extended(SendSettings send_settings,
                                                     const HttpRequest* const http_request,
                                                     const ConnectionContext* const context,
                                                     ParsedURLPath path) {

	UNUSED(path);

	StringBuilder* htmlStringBuilder =
	    http_request_to_html(http_request, is_secure_context(context), send_settings);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&htmlStringBuilder),
		                          .mime_type = MIME_TYPE_HTML,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend json_executor_fn_extended(SendSettings send_settings,
                                                    const HttpRequest* const http_request,
                                                    const ConnectionContext* const context,
                                                    ParsedURLPath path) {

	UNUSED(path);

	StringBuilder* jsonStringBuilder =
	    http_request_to_json(http_request, is_secure_context(context), send_settings);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&jsonStringBuilder),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend static_executor_fn(ParsedURLPath path) {

	UNUSED(path);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_static_string("{\"static\":true}"),
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
	size_t minimumSize =
	    1 << 20; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	while(string_builder_get_string_size(string_builder) < minimumSize) {
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

	UNUSED(path);

	StringBuilder* string_builder = get_random_json_string_builder(false);

	HTTPResponseToSend result = { .status = HttpStatusOk,
		                          .body = http_response_body_from_string_builder(&string_builder),
		                          .mime_type = MIME_TYPE_JSON,
		                          .additional_headers = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend huge_pretty_executor_fn(ParsedURLPath path) {

	// TODO(Totto): get the search paramaters and use them to determine if we need pretty json
	UNUSED(path);

	StringBuilder* string_builder = get_random_json_string_builder(true);

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

		HTTPRoute shutdown = { .method = HTTPRequestRouteMethodGet,
			                   .path = "/shutdown",
			                   .data = (HTTPRouteData){
			                       .type = HTTPRouteTypeSpecial,
			                       .data = { .special = HTTPRouteSpecialDataShutdown } } };

		stbds_arrput(routes, shutdown);
	}

	{

		// index (/)

		HTTPRoute index = { .method = HTTPRequestRouteMethodGet,
			                .path = "/",
			                .data = (HTTPRouteData){
			                    .type = HTTPRouteTypeNormal,
			                    .data = { .normal = (HTTPRouteFn){
			                                  .type = HTTPRouteFnTypeExecutorExtended,
			                                  .fn = { .executor_extended =
			                                              index_executor_fn_extended } } } } };

		stbds_arrput(routes, index);
	}

	{

		// ws

		HTTPRoute wsRoute = { .method = HTTPRequestRouteMethodGet,
			                  .path = "/ws",
			                  .data = (HTTPRouteData){
			                      .type = HTTPRouteTypeSpecial,
			                      .data = { .special = HTTPRouteSpecialDataWs } } };

		stbds_arrput(routes, wsRoute);
	}

	{

		// ws fragmented

		HTTPRoute wsFragmented = { .method = HTTPRequestRouteMethodGet,
			                       .path = "/ws/fragmented",
			                       .data = (HTTPRouteData){
			                           .type = HTTPRouteTypeSpecial,
			                           .data = { .special = HTTPRouteSpecialDataWsFragmented } } };

		stbds_arrput(routes, wsFragmented);
	}

	{

		// json

		HTTPRoute json = { .method = HTTPRequestRouteMethodGet,
			               .path = "/json",
			               .data = (HTTPRouteData){
			                   .type = HTTPRouteTypeNormal,
			                   .data = { .normal = (HTTPRouteFn){
			                                 .type = HTTPRouteFnTypeExecutorExtended,
			                                 .fn = { .executor_extended =
			                                             json_executor_fn_extended } } } } };

		stbds_arrput(routes, json);
	}

	{

		// static

		HTTPRoute json = { .method = HTTPRequestRouteMethodGet,
			               .path = "/static",
			               .data = (HTTPRouteData){
			                   .type = HTTPRouteTypeNormal,
			                   .data = { .normal = (HTTPRouteFn){
			                                 .type = HTTPRouteFnTypeExecutor,
			                                 .fn = { .executor = static_executor_fn } } } } };

		stbds_arrput(routes, json);
	}

	{

		// huge

		HTTPRoute json = { .method = HTTPRequestRouteMethodGet,
			               .path = "/huge",
			               .data = (HTTPRouteData){
			                   .type = HTTPRouteTypeNormal,
			                   .data = { .normal = (HTTPRouteFn){
			                                 .type = HTTPRouteFnTypeExecutor,
			                                 .fn = { .executor = huge_executor_fn } } } } };

		stbds_arrput(routes, json);
	}

	// TODO(Totto): support ? search paraamters and make huge use them
	{

		// huge/ pretty

		HTTPRoute json = { .method = HTTPRequestRouteMethodGet,
			               .path = "/huge/pretty",
			               .data = (HTTPRouteData){
			                   .type = HTTPRouteTypeNormal,
			                   .data = { .normal = (HTTPRouteFn){
			                                 .type = HTTPRouteFnTypeExecutor,
			                                 .fn = { .executor = huge_pretty_executor_fn } } } } };

		stbds_arrput(routes, json);
	}

	return routes;
}

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes routes) {
	RouteManager* route_manager = malloc(sizeof(RouteManager));

	if(!route_manager) {
		return NULL;
	}

	route_manager->routes = routes;

	return route_manager;
}

void free_route_manager(RouteManager* route_manager) {

	stbds_arrfree(route_manager->routes);

	free(route_manager);
}

NODISCARD static bool is_matching(HTTPRequestRouteMethod routeMethod, HTTPRequestMethod method) {

	if(routeMethod == HTTPRequestRouteMethodGet && method == HTTPRequestMethodHead) {
		return true;
	}

	if(routeMethod == HTTPRequestRouteMethodGet && method == HTTPRequestMethodGet) {
		return true;
	}

	if(routeMethod == HTTPRequestRouteMethodPost && method == HTTPRequestMethodPost) {
		return true;
	}

	return false;
}

struct SelectedRouteImpl {
	HTTPRoute route;
	ParsedURLPath path;
};

NODISCARD static SelectedRoute* selected_route_from_data(HTTPRoute route, ParsedURLPath path) {
	SelectedRoute* selected_route = malloc(sizeof(SelectedRoute));

	if(!selected_route) {
		return NULL;
	}

	selected_route->route = route;
	selected_route->path = path;

	return selected_route;
}

void free_selected_route(SelectedRoute* selected_route) {
	free(selected_route);
}

NODISCARD SelectedRoute*
route_manager_get_route_for_request(const RouteManager* const route_manager,
                                    const HttpRequest* const request) {

	for(size_t i = 0; i < stbds_arrlenu(route_manager->routes); ++i) {
		HTTPRoute route = route_manager->routes[i];

		if(is_matching(route.method, request->head.request_line.method)) {

			if(route.path == NULL) {
				return selected_route_from_data(route, request->head.request_line.path);
			}

			if(strcmp(route.path, request->head.request_line.path.path) == 0) {
				return selected_route_from_data(route, request->head.request_line.path);
			}
		}
	}

	return NULL;
}

NODISCARD HTTPSelectedRoute get_selected_route_data(const SelectedRoute* const route) {

	return (HTTPSelectedRoute){ .data = route->route.data, .path = route->path };
}

NODISCARD int
route_manager_execute_route(HTTPRouteFn route, const ConnectionDescriptor* const descriptor,
                            SendSettings send_settings, const HttpRequest* const http_request,
                            const ConnectionContext* const context, ParsedURLPath path) {

	HTTPResponseToSend response = {};

	switch(route.type) {
		case HTTPRouteFnTypeExecutor: {
			response = route.fn.executor(path);

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

	int result = send_http_message_to_connection_advanced(descriptor, response, send_settings,
	                                                 http_request->head);

	return result;
}
