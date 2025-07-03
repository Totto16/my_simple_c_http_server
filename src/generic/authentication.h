

#pragma once

#include "./hash.h"
#include "utils/utils.h"

#include <stb/ds.h>

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

NODISCARD AuthenticationProviders* initialize_authentication_providers(void);

NODISCARD bool add_authentication_provider(AuthenticationProviders* auth_providers,
                                           AuthenticationProvider* provider);

NODISCARD AuthenticationProvider* initialize_simple_authentication_provider(void);

void free_authentication_provider(AuthenticationProvider* auth_provider);

NODISCARD AuthenticationProvider* initialize_system_authentication_provider(void);

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* simple_authentication_provider, char* username, char* password,
    char* role);

NODISCARD bool add_user_to_simple_authentication_provider_data_password_hash_salted(
    AuthenticationProvider* simple_authentication_provider, char* username,
    HashSaltResultType* hash_salted_password, char* role);

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
	char* username;
	char* role;
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
    const AuthenticationProviders* auth_providers, char* username, char* password);
