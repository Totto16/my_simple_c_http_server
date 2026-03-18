

#pragma once

#include "./hash.h"
#include "utils/utils.h"

#include <tstr.h>

typedef struct AuthenticationProvidersImpl AuthenticationProviders;

typedef struct AuthenticationProviderImpl AuthenticationProvider;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	AuthenticationProviderTypeSimple = 0,
	AuthenticationProviderTypeSystem,
} AuthenticationProviderType;

NODISCARD const char* get_name_for_auth_provider_type(AuthenticationProviderType type);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	UserRoleNone = 0,
	UserRoleAdmin,
	UserRoleUser,
} UserRole;

NODISCARD const char* get_name_for_user_role(UserRole role);

NODISCARD AuthenticationProviders* initialize_authentication_providers(void);

NODISCARD bool add_authentication_provider(AuthenticationProviders* auth_providers,
                                           AuthenticationProvider* provider);

NODISCARD AuthenticationProvider* initialize_simple_authentication_provider(void);

void free_authentication_provider(AuthenticationProvider* auth_provider);

NODISCARD AuthenticationProvider* initialize_system_authentication_provider(void);

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* simple_authentication_provider, const tstr* username,
    const tstr* password, UserRole role);

NODISCARD bool add_user_to_simple_authentication_provider_data_password_hash_salted(
    AuthenticationProvider* simple_authentication_provider, const tstr* username,
    HashSaltResultType* hash_salted_password, UserRole role);

void free_authentication_providers(AuthenticationProviders* auth_providers);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	AuthenticationValidityNoSuchUser = 0,
	AuthenticationValidityWrongPassword,
	AuthenticationValidityOk,
	AuthenticationValidityError,
} AuthenticationValidity;

typedef struct {
	tstr username;
	UserRole role;
} AuthUser;

typedef struct {
	AuthUser user;
	AuthenticationProviderType provider_type;
} AuthUserWithContext;

typedef struct {
	AuthenticationValidity validity;
	union {
		AuthUserWithContext ok;
		struct {
			const char* error_message;
		} error;
	} data;
} AuthenticationFindResult;

NODISCARD AuthenticationFindResult authentication_providers_find_user_with_password(
    const AuthenticationProviders* auth_providers, const tstr* username, const tstr* password);
