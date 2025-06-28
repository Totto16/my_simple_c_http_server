

#pragma once

#include "./http_protocol.h"
#include "generic/secure.h"
#include "utils/utils.h"
#include <stb/ds.h>

typedef struct RouteManagerImpl RouteManager;

// TODO: support oute, that just returns a response, that are paramaters to
// sendHTTPMessageToConnection, since thaht function needs to be refactored anyway!

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteFnTypeExecutor = 0,
	HTTPRouteFnTypeExecutorExtended
} HTTPRouteFnType;

typedef int (*HTTPRouteFnExecutor)(const ConnectionDescriptor* const descriptor,
                                   SendSettings send_settings);

typedef int (*HTTPRouteFnExecutorExtended)(const ConnectionDescriptor* const descriptor,
                                           SendSettings send_settings,
                                           const HttpRequest* const httpRequest,
                                           const ConnectionContext* const context);

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

NODISCARD SelectedRoute*
route_manager_get_route_for_request(const RouteManager* const routerManager,
                                    const HttpRequest* const request);

NODISCARD HTTPRouteData get_route_data(const SelectedRoute* const route);

NODISCARD int route_manager_execute_route(HTTPRouteFn route,
                                          const ConnectionDescriptor* const descriptor,
                                          SendSettings send_settings,
                                          const HttpRequest* const httpRequest,
                                          const ConnectionContext* const context);
