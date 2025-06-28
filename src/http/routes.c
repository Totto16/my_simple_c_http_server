
#include "./routes.h"
#include "./http_protocol.h"
#include "http/send.h"

struct RouteManagerImpl {
	HTTPRoutes routes;
};

static HTTPResponseToSend index_executor_fn_extended(SendSettings send_settings,
                                                     const HttpRequest* const httpRequest,
                                                     const ConnectionContext* const context) {

	HTTPResponseToSend result = { .status = HTTP_STATUS_OK,
		                          .body = httpResponseBodyFromStringBuilder(httpRequestToHtml(
		                              httpRequest, is_secure_context(context), send_settings)),
		                          .MIMEType = MIME_TYPE_HTML,
		                          .additionalHeaders = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend json_executor_fn_extended(SendSettings send_settings,
                                                    const HttpRequest* const httpRequest,
                                                    const ConnectionContext* const context) {

	HTTPResponseToSend result = { .status = HTTP_STATUS_OK,
		                          .body = httpResponseBodyFromStringBuilder(httpRequestToJSON(
		                              httpRequest, is_secure_context(context), send_settings)),
		                          .MIMEType = MIME_TYPE_JSON,
		                          .additionalHeaders = STBDS_ARRAY_EMPTY };
	return result;
}

static HTTPResponseToSend static_executor_fn() {

	HTTPResponseToSend result = { .status = HTTP_STATUS_OK,
		                          .body = httpResponseBodyFromStaticString("{\"static\":true}"),
		                          .MIMEType = MIME_TYPE_JSON,
		                          .additionalHeaders = STBDS_ARRAY_EMPTY };
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

		HTTPRoute ws = { .method = HTTPRequestRouteMethodGet,
			             .path = "/ws",
			             .data = (HTTPRouteData){ .type = HTTPRouteTypeSpecial,
			                                      .data = { .special = HTTPRouteSpecialDataWs } } };

		stbds_arrput(routes, ws);
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

	return routes;
}

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes routes) {
	RouteManager* routeManager = malloc(sizeof(RouteManager));

	if(!routeManager) {
		return NULL;
	}

	routeManager->routes = routes;

	return routeManager;
}

void free_route_manager(RouteManager* routeManager) {

	stbds_arrfree(routeManager->routes);

	free(routeManager);
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
};

NODISCARD static SelectedRoute* selected_route_from_data(HTTPRoute route) {
	SelectedRoute* selected_route = malloc(sizeof(SelectedRoute));

	if(!selected_route) {
		return NULL;
	}

	selected_route->route = route;

	return selected_route;
}

void free_selected_route(SelectedRoute* selected_route) {
	free(selected_route);
}

NODISCARD SelectedRoute*
route_manager_get_route_for_request(const RouteManager* const routerManager,
                                    const HttpRequest* const request) {

	for(size_t i = 0; i < stbds_arrlenu(routerManager->routes); ++i) {
		HTTPRoute route = routerManager->routes[i];

		if(is_matching(route.method, request->head.requestLine.method)) {

			if(route.path == NULL) {
				return selected_route_from_data(route);
			}

			if(strcmp(route.path, request->head.requestLine.URI) == 0) {
				return selected_route_from_data(route);
			}
		}
	}

	return NULL;
}

NODISCARD HTTPRouteData get_route_data(const SelectedRoute* const route) {

	return route->route.data;
}

NODISCARD int route_manager_execute_route(HTTPRouteFn route,
                                          const ConnectionDescriptor* const descriptor,
                                          SendSettings send_settings,
                                          const HttpRequest* const httpRequest,
                                          const ConnectionContext* const context) {

	HTTPResponseToSend response = {};

	switch(route.type) {
		case HTTPRouteFnTypeExecutor: {
			response = route.fn.executor();

			break;
		}
		case HTTPRouteFnTypeExecutorExtended: {
			response = route.fn.executor_extended(send_settings, httpRequest, context);
			break;
		}
		default: {
			return -11;
			break;
		}
	}

	httpResponseAdjustToRequestMethod(&response, httpRequest->head.requestLine.method);
	int result = sendHTTPMessageToConnection(descriptor, response, send_settings);

	return result;
}

void httpResponseAdjustToRequestMethod(HTTPResponseToSend* responsePtr, HTTPRequestMethod method) {

	if(method == HTTPRequestMethodHead) {
		responsePtr->MIMEType = NULL;
		freeSizedBuffer(responsePtr->body.body);
		responsePtr->body = httpResponseBodyEmpty();
	}
}
