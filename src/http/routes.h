

#pragma once

#include "./http_protocol.h"
#include "./send.h"
#include "generic/secure.h"
#include "utils/utils.h"
#include <stb/ds.h>

typedef struct RouteManagerImpl RouteManager;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteFnTypeExecutor = 0,
	HTTPRouteFnTypeExecutorExtended
} HTTPRouteFnType;

typedef HTTPResponseToSend (*HTTPRouteFnExecutor)(ParsedURLPath path);

typedef HTTPResponseToSend (*HTTPRouteFnExecutorExtended)(SendSettings send_settings,
                                                          const HttpRequest* const http_request,
                                                          const ConnectionContext* const context,
                                                          ParsedURLPath path);

// TODO(Totto): add support for file routes, that e.g. just resolve to a file and retrieve the
// mime-type from it and send it, for e.g. static file serving
typedef struct {
	HTTPRouteFnType type;
	union {
		HTTPRouteFnExecutor executor;
		HTTPRouteFnExecutorExtended executor_extended;
	} fn;
} HTTPRouteFn;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteTypeNormal = 0,
	HTTPRouteTypeSpecial
} HTTPRouteType;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteSpecialDataShutdown = 0,
	HTTPRouteSpecialDataWs,
	HTTPRouteSpecialDataWsFragmented,
} HTTPRouteSpecialData;

typedef struct {
	HTTPRouteType type;
	union {
		HTTPRouteSpecialData special;
		HTTPRouteFn normal;
	} data;
} HTTPRouteData;

typedef struct {
	HTTPRouteData data;
	ParsedURLPath path;
} HTTPSelectedRoute;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRequestRouteMethodGet = 0,
	HTTPRequestRouteMethodPost,
} HTTPRequestRouteMethod;

typedef struct {
	HTTPRequestRouteMethod method;
	char* path;
	HTTPRouteData data;
} HTTPRoute;

typedef STBDS_ARRAY(HTTPRoute) HTTPRoutes;

HTTPRoutes get_default_routes(void);

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes routes);

void free_route_manager(RouteManager* routeManager);

typedef struct SelectedRouteImpl SelectedRoute;

void free_selected_route(SelectedRoute* selected_route);

NODISCARD SelectedRoute* route_manager_get_route_for_request(const RouteManager* routerManager,
                                                             const HttpRequest* request);

NODISCARD HTTPSelectedRoute get_selected_route_data(const SelectedRoute* route);

NODISCARD int route_manager_execute_route(HTTPRouteFn route, const ConnectionDescriptor* descriptor,
                                          SendSettings send_settings,
                                          const HttpRequest* httpRequest,
                                          const ConnectionContext* context, ParsedURLPath path);
