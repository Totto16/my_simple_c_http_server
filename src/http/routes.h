

#pragma once

#include "./http_protocol.h"
#include "./send.h"
#include "generic/authentication.h"
#include "generic/secure.h"
#include "utils/utils.h"

#include <stb/ds.h>

typedef struct RouteManagerImpl RouteManager;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteFnTypeExecutor = 0,
	HTTPRouteFnTypeExecutorExtended,
	HTTPRouteFnTypeExecutorAuth
} HTTPRouteFnType;

typedef HTTPResponseToSend (*HTTPRouteFnExecutor)(ParsedURLPath path);

typedef HTTPResponseToSend (*HTTPRouteFnExecutorAuth)(ParsedURLPath path, AuthUserWithContext user);

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
		HTTPRouteFnExecutorAuth executor_auth;
	} fn;
} HTTPRouteFn;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteTypeNormal = 0,
	HTTPRouteTypeSpecial,
	HTTPRouteTypeInternal,
	HTTPRouteTypeServeFolder
} HTTPRouteType;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteSpecialDataTypeShutdown = 0,
	HTTPRouteSpecialDataTypeWs,
} HTTPRouteSpecialDataType;

typedef struct {
	HTTPRouteSpecialDataType type;
} HTTPRouteSpecialData;

typedef struct {
	HTTPResponseToSend send;
} HTTPRouteInternal;

typedef struct {
	char* folder_path;
} HTTPRouteServeFolder;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPAuthorizationTypeNone = 0,
	HTTPAuthorizationTypeSimple,
	HTTPAuthorizationTypeComplicated,
} HTTPAuthorizationType;

typedef

    struct {
	int todo; // TODO(Totto): support mapping roles and or users to if the request is
	          // possible or not, via a callback function, that gets the user and returns
	          // true or false!
} HTTPAuthorizationComplicatedData;

typedef struct {
	HTTPAuthorizationType type;
	union {
		HTTPAuthorizationComplicatedData complicated;
	} data;
} HTTPAuthorization;

typedef struct {
	HTTPRouteType type;
	union {
		HTTPRouteSpecialData special;
		HTTPRouteInternal internal;
		HTTPRouteFn normal;
		HTTPRouteServeFolder serve_folder;
	} data;
} HTTPRouteData;

typedef struct {
	HTTPRouteData data;
	ParsedURLPath path;
	AuthUserWithContext* auth_user;
} HTTPSelectedRoute;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRequestRouteMethodGet = 0,
	HTTPRequestRouteMethodPost,
} HTTPRequestRouteMethod;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRoutePathTypeExact = 0,
	HTTPRoutePathTypeStartsWith,
} HTTPRoutePathType;

typedef struct {
	HTTPRoutePathType type;
	char* data;
} HTTPRoutePath;

typedef struct {
	HTTPRequestRouteMethod method;
	HTTPRoutePath path;
	HTTPRouteData data;
	HTTPAuthorization auth;
} HTTPRoute;

typedef STBDS_ARRAY(HTTPRoute) HTTPRoutes;

NODISCARD HTTPRoutes get_default_routes(void);

NODISCARD HTTPRoutes get_webserver_test_routes(void);

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes routes,
                                                 const AuthenticationProviders* auth_providers);

void free_route_manager(RouteManager* route_manager);

typedef struct SelectedRouteImpl SelectedRoute;

void free_selected_route(SelectedRoute* selected_route);

NODISCARD SelectedRoute* route_manager_get_route_for_request(const RouteManager* route_manager,
                                                             const HttpRequest* request);

NODISCARD HTTPSelectedRoute get_selected_route_data(const SelectedRoute* route);

NODISCARD int route_manager_execute_route(HTTPRouteFn route, const ConnectionDescriptor* descriptor,
                                          SendSettings send_settings,
                                          const HttpRequest* http_request,
                                          const ConnectionContext* context, ParsedURLPath path,
                                          AuthUserWithContext* auth_user);
