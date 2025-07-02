

#include "./authentication.h"

typedef struct {
	char* username;
	HashSaltResultType hash_salted_password;
	char* role;
} SimpleAccountEntry;

typedef struct {
	STBDS_ARRAY(SimpleAccountEntry) entries;
	HashSaltSettings settings;
} SimpleAuthenticationProviderData;

typedef struct {
	int todo;
} SystemAuthenticationProviderData;

struct AuthenticationProviderImpl {
	AuthenticationProviderType type;
	union {
		SimpleAuthenticationProviderData simple;
		SystemAuthenticationProviderData system;
	} data;
};

struct AuthenticationProvidersImpl {
	STBDS_ARRAY(AuthenticationProvider*) providers;
};

NODISCARD const char* get_name_for_auth_provider_type(AuthenticationProviderType type) {
	switch(type) {
		case AuthenticationProviderTypeSimple: return "simple authentication provider";
		case AuthenticationProviderTypeSystem: return "system authentication provider";
		default: return "<unknown>";
	}
}

NODISCARD AuthenticationProviders* initialize_authentication_providers(void) {
	AuthenticationProviders* auth_providers = malloc(sizeof(AuthenticationProviders));

	if(!auth_providers) {
		return NULL;
	}

	auth_providers->providers = STBDS_ARRAY_EMPTY;

	return auth_providers;
}

#define TODO_HASH_SETTINGS 20

NODISCARD AuthenticationProvider* initialize_simple_authentication_provider(void) {

	AuthenticationProvider* auth_provider = malloc(sizeof(AuthenticationProvider));

	if(!auth_provider) {
		return NULL;
	}

	auth_provider->type = AuthenticationProviderTypeSimple;
	auth_provider->data.simple =
	    (SimpleAuthenticationProviderData){ .entries = STBDS_ARRAY_EMPTY,
		                                    .settings = { .todo = TODO_HASH_SETTINGS } };

	return auth_provider;
}

NODISCARD AuthenticationProvider* initialize_system_authentication_provider(void) {
	AuthenticationProvider* auth_provider = malloc(sizeof(AuthenticationProvider));

	if(!auth_provider) {
		return NULL;
	}

	auth_provider->type = AuthenticationProviderTypeSystem;
	auth_provider->data.system = (SystemAuthenticationProviderData){ .todo = 1 };

	return auth_provider;
}

NODISCARD bool add_authentication_provider(AuthenticationProviders* auth_providers,
                                           AuthenticationProvider* provider) {

	if(!provider) {
		return false;
	}

	stbds_arrput(auth_providers->providers,
	             provider); // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
	return true;
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* simple_authentication_provider, char* username, char* password,
    char* role) {

	if(simple_authentication_provider->type != AuthenticationProviderTypeSimple) {
		return false;
	}

	SimpleAuthenticationProviderData* data = &simple_authentication_provider->data.simple;

	SizedBuffer hash_salted_password = hash_salt_string(data->settings, password);

	return add_user_to_simple_authentication_provider_data_password_hash_salted(
	    simple_authentication_provider, username, hash_salted_password, role);
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_hash_salted(
    AuthenticationProvider* simple_authentication_provider, char* username,
    SizedBuffer hash_salted_password, char* role) {

	if(simple_authentication_provider->type != AuthenticationProviderTypeSimple) {
		return false;
	}

	SimpleAuthenticationProviderData* data = &simple_authentication_provider->data.simple;

	SimpleAccountEntry entry = { .username = username,
		                         .hash_salted_password = hash_salted_password,
		                         .role = role };

	stbds_arrput(data->entries, entry);
	return true;
}

static void free_simple_authentication_provider(SimpleAuthenticationProviderData data) {
	stbds_arrfree(data.entries);
}

static void free_system_authentication_provider(SystemAuthenticationProviderData data) {
	UNUSED(data);
	// TODO
}

static void free_authentication_provider(AuthenticationProvider* auth_provider) {

	switch(auth_provider->type) {
		case AuthenticationProviderTypeSimple: {
			free_simple_authentication_provider(auth_provider->data.simple);
			free(auth_provider);
			break;
		}
		case AuthenticationProviderTypeSystem: {
			free_system_authentication_provider(auth_provider->data.system);
			free(auth_provider);
			break;
		}
		default: break;
	}
}

void free_authentication_providers(AuthenticationProviders* auth_providers) {

	for(size_t i = 0; i < stbds_arrlenu(auth_providers->providers); ++i) {
		free_authentication_provider(auth_providers->providers[i]);
	}

	stbds_arrfree(auth_providers->providers);

	free(auth_providers);
}

static AuthenticationFindResult
authentication_provider_simple_find_user_with_password(const AuthenticationProvider* auth_provider,
                                                       char* username, char* password) {

	UNUSED(auth_provider);

	UNUSED(username);
	UNUSED(password);

	return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
		                               .data = { .error = { .error_message = "TODO" } } };
}

static AuthenticationFindResult authentication_provider_system_find_user_with_password(
    const AuthenticationProvider* auth_provider,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password) {

	UNUSED(auth_provider);

	// TODO(Totto): https://stackoverflow.com/questions/64184960/pam-authenticate-a-user-in-c
	//  and https://github.com/linux-pam/linux-pam/blob/master/examples/check_user.c

	UNUSED(username);
	UNUSED(password);

	return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
		                               .data = { .error = { .error_message = "TODO" } } };
}

NODISCARD AuthenticationFindResult authentication_providers_find_user_with_password(
    const AuthenticationProviders* auth_providers,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password) {

	AuthenticationFindResult last_result = (AuthenticationFindResult){
		.validity = AuthenticationValidityError,
		.data = { .error = { .error_message = "no single provider registered" } }
	};

	for(size_t i = 0; stbds_arrlenu(auth_providers->providers); ++i) {
		AuthenticationProvider* provider = auth_providers->providers[i];

		AuthenticationFindResult result;

		switch(provider->type) {
			case AuthenticationProviderTypeSimple: {
				result = authentication_provider_simple_find_user_with_password(provider, username,
				                                                                password);
				break;
			}
			case AuthenticationProviderTypeSystem: {
				result = authentication_provider_system_find_user_with_password(provider, username,
				                                                                password);
				break;
			}
			default:
				result = (AuthenticationFindResult){
					.validity = AuthenticationValidityError,
					.data = { .error = { .error_message = "unrecognized provider type" } }
				};
				break;
		}

		last_result = result;

		if(last_result.validity == AuthenticationValidityOk) {
			break;
		}
	}

	return last_result;
}
