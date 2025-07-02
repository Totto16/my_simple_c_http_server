

#pragma once

#include "./hash.h"
#include "utils/sized_buffer.h"
#include "utils/utils.h"

#include <stb/ds.h>

typedef struct AuthenticationProvidersImpl AuthenticationProviders;

typedef struct AuthenticationProviderImpl AuthenticationProvider;

NODISCARD AuthenticationProviders* initialize_authentication_providers(void);

NODISCARD bool add_authentication_provider(AuthenticationProviders* auth_providers,
                                           AuthenticationProvider* provider);

NODISCARD AuthenticationProvider* initialize_simple_authentication_provider(void);

NODISCARD AuthenticationProvider* initialize_system_authentication_provider(void);

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* simple_authentication_provider, char* username, char* password,
    char* role);

NODISCARD bool add_user_to_simple_authentication_provider_data_password_hash_salted(
    AuthenticationProvider* simple_authentication_provider_data, char* username,
    HashSaltResultType hash_salted_password, char* role);

void free_authentication_providers(AuthenticationProviders* auth_providers);
