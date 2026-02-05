

#pragma once

#include "./protocol.h"
#include "./send.h"
#include "generic/authentication.h"
#include "generic/ip.h"
#include "generic/secure.h"
#include "utils/utils.h"

#include <tvec.h>

typedef struct RouteManagerImpl RouteManager;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteFnTypeExecutor = 0,
	HTTPRouteFnTypeExecutorExtended,
	HTTPRouteFnTypeExecutorAuth
} HTTPRouteFnType;

typedef HTTPResponseToSend (*HTTPRouteFnExecutor)(ParsedURLPath path, bool send_body);

typedef HTTPResponseToSend (*HTTPRouteFnExecutorAuth)(ParsedURLPath path, AuthUserWithContext user,
                                                      bool send_body);

typedef HTTPResponseToSend (*HTTPRouteFnExecutorExtended)(SendSettings send_settings,
                                                          const HttpRequest http_request,
                                                          const ConnectionContext* const context,
                                                          ParsedURLPath path, void* data);

typedef struct {
	void* data;
	HTTPRouteFnExecutorExtended executor_extended;
} HTTPRouteFnExecutorExtendedData;

typedef struct {
	HTTPRouteFnType type;
	union {
		HTTPRouteFnExecutor fn_executor;
		HTTPRouteFnExecutorExtendedData extended_data;
		HTTPRouteFnExecutorAuth fn_executor_auth;
	} value;
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

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRouteServeFolderTypeRelative /* NGINX 'root'*/ = 0,
	// this means, that if you server the folder on the path "/test/", and you request
	// "/hello/test.txt" you will get "<YOUR_FOLDER>/test/hello/test.txt"
	HTTPRouteServeFolderTypeAbsolute /* NGINX 'alias'*/,
	// this means, that if you server the folder
	// on the path "/test/", and you request
	// "/hello/test.txt" you will get
	// "<YOUR_FOLDER>/hello/test.txt"

	// see also root vs alias in NGINX, if you serve from "/" it doesn't make any difference
	// see:
	// https://stackoverflow.com/questions/10631933/nginx-static-file-serving-confusion-with-root-alias
} HTTPRouteServeFolderType;

typedef struct {
	HTTPRouteServeFolderType type;
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
	} value;
} HTTPRouteData;

typedef struct {
	HTTPRouteData data;
	ParsedURLPath path;
	const char* original_path;
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
	const char* data;
} HTTPRoutePath;

typedef struct {
	HTTPRequestRouteMethod method;
	HTTPRoutePath path;
	HTTPRouteData data;
	HTTPAuthorization auth;
} HTTPRoute;

TVEC_DEFINE_VEC_TYPE(HTTPRoute)

typedef TVEC_TYPENAME(HTTPRoute) HTTPRoutesArray;

typedef void (*FreeFnImpl)(void*);

typedef struct {
	void* data;
	FreeFnImpl fn;
} HTTPFreeFn;

TVEC_DEFINE_VEC_TYPE(HTTPFreeFn)

typedef TVEC_TYPENAME(HTTPFreeFn) HTTPFreeFns;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	HTTPRequestProxyTypePre = 0,
	HTTPRequestProxyTypePost,
} HTTPRequestProxyType;

typedef void (*HTTPRequestProxyPreFn)(const HttpRequest http_request, IPAddress address,
                                      void* data);

typedef void (*HTTPRequestProxyPostFn)(const HttpRequest http_request,
                                       const HTTPResponseToSend response, IPAddress address,
                                       void* data);

typedef struct {
	HTTPRequestProxyType type;
	union {
		HTTPRequestProxyPreFn pre;
		HTTPRequestProxyPostFn post;
	} value;
	void* data;
} HTTPRequestProxy;

TVEC_DEFINE_VEC_TYPE(HTTPRequestProxy)

typedef TVEC_TYPENAME(HTTPRequestProxy) HTTPRequestProxies;

typedef struct {
	HTTPRoutesArray routes;
	HTTPRequestProxies proxies;
	HTTPFreeFns free_fns;
} HTTPRoutes;

NODISCARD HTTPRoutes* get_default_routes(void);

NODISCARD HTTPRoutes* get_webserver_test_routes(void);

NODISCARD RouteManager* initialize_route_manager(HTTPRoutes* routes,
                                                 const AuthenticationProviders* auth_providers);

void free_route_manager(RouteManager* route_manager);

typedef struct SelectedRouteImpl SelectedRoute;

void free_selected_route(SelectedRoute* selected_route);

NODISCARD SelectedRoute* route_manager_get_route_for_request(const RouteManager* route_manager,
                                                             HttpRequestProperties http_properties,
                                                             HttpRequest request,
                                                             IPAddress address);

NODISCARD HTTPSelectedRoute get_selected_route_data(const SelectedRoute* route);

NODISCARD int route_manager_execute_route(const RouteManager* route_manager, HTTPRouteFn route,
                                          const ConnectionDescriptor* descriptor,
                                          SendSettings send_settings, HttpRequest http_request,
                                          const ConnectionContext* context, ParsedURLPath path,
                                          AuthUserWithContext* auth_user, IPAddress address);
