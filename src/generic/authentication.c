

#include "./authentication.h"

#ifdef _SIMPLE_SERVER_USE_BCRYPT
#include <bcrypt.h>
#endif

#include "utils/log.h"

typedef struct {
	char* key;
	HashSaltResultType* hash_salted_password;
	char* role;
} SimpleAccountEntry;

typedef struct {
	STBDS_HASH_MAP(SimpleAccountEntry) entries;
	HashSaltSettings settings;
} SimpleAuthenticationProviderData;

typedef struct {
	int todo; // TODO(Totto): implement
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

#ifndef _SIMPLE_SERVER_USE_BCRYPT
	return NULL;
#else

	AuthenticationProvider* auth_provider = malloc(sizeof(AuthenticationProvider));

	if(!auth_provider) {
		return NULL;
	}

	auth_provider->type = AuthenticationProviderTypeSimple;
	auth_provider->data.simple =
	    (SimpleAuthenticationProviderData){ .entries = STBDS_HASH_MAP_EMPTY,
		                                    .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR,
		                                                  .use_sha512 = true } };

	return auth_provider;
#endif
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

	stbds_arrput( // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
	    auth_providers->providers, provider);
	return true;
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* simple_authentication_provider,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password, char* role) {

#ifndef _SIMPLE_SERVER_USE_BCRYPT
	UNUSED(simple_authentication_provider);
	UNUSED(username);
	UNUSED(password);
	UNUSED(role);
	return false;
#else

	if(simple_authentication_provider->type != AuthenticationProviderTypeSimple) {
		return false;
	}

	SimpleAuthenticationProviderData* data = &simple_authentication_provider->data.simple;

	HashSaltResultType* hash_salted_password = hash_salt_string(data->settings, password);

	return add_user_to_simple_authentication_provider_data_password_hash_salted(
	    simple_authentication_provider, username, hash_salted_password, role);
#endif
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_hash_salted(
    AuthenticationProvider* simple_authentication_provider, char* username,
    HashSaltResultType* hash_salted_password, char* role) {

	if(simple_authentication_provider->type != AuthenticationProviderTypeSimple) {
		return false;
	}

	SimpleAuthenticationProviderData* data = &simple_authentication_provider->data.simple;

	SimpleAccountEntry entry = { .key = strdup(username),
		                         .hash_salted_password = hash_salted_password,
		                         .role = strdup(role) };

	stbds_shputs(data->entries, entry);
	return true;
}

static void free_simple_authentication_provider(SimpleAuthenticationProviderData data) {

#ifndef _SIMPLE_SERVER_USE_BCRYPT
	UNUSED(data);
#else
	size_t hm_length = stbds_shlenu(data.entries);

	for(size_t i = 0; i < hm_length; ++i) {
		SimpleAccountEntry entry = data.entries[i];

		free_hash_salted_result(entry.hash_salted_password);
		free(entry.key);
		free(entry.role);
	}

	stbds_shfree(data.entries);

#endif
}

static void free_system_authentication_provider(SystemAuthenticationProviderData data) {
	UNUSED(data);
	// TODO(Totto): implement
}

void free_authentication_provider(AuthenticationProvider* auth_provider) {
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

#ifdef _SIMPLE_SERVER_USE_BCRYPT

NODISCARD static SimpleAccountEntry*
find_user_by_name_simple(SimpleAuthenticationProviderData* data, char* username) {

	if(data->entries == STBDS_HASH_MAP_EMPTY) {
		// note: if hash_map is NULL stbds_shgeti allocates a new value, that is never populated to
		// the original SimpleAccountEntry value, as this is a struct copy!
		return NULL;
	}

	int index = stbds_shgeti(data->entries, username);

	if(index < 0) {
		return NULL;
	}

	return &data->entries[index];
}

NODISCARD static AuthenticationFindResult authentication_provider_simple_find_user_with_password(
    AuthenticationProvider* auth_provider,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password) {

	if(auth_provider->type != AuthenticationProviderTypeSimple) {

		return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
			                               .data = { .error = { .error_message =
			                                                        "Implementation error" } } };
	}

	SimpleAuthenticationProviderData* data = &auth_provider->data.simple;

	SimpleAccountEntry* entry = find_user_by_name_simple(data, username);

	if(!entry) {
		return (AuthenticationFindResult){ .validity = AuthenticationValidityNoSuchUser,
			                               .data = {} };
	}

	bool is_valid_pw = is_string_equal_to_hash_salted_string(data->settings, password,
	                                                         entry->hash_salted_password);

	if(!is_valid_pw) {
		return (AuthenticationFindResult){ .validity = AuthenticationValidityWrongPassword,
			                               .data = {} };
	}

	// TODO(Totto): maybe don't allocate this?
	AuthUser user = { .username = strdup(entry->key), .role = strdup(entry->role) };

	return (AuthenticationFindResult){
		.validity = AuthenticationValidityOk,
		.data = { .ok = { .provider_type = AuthenticationProviderTypeSimple, .user = user } }
	};

	return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
		                               .data = { .error = { .error_message = "TODO" } } };
}

#endif

NODISCARD static AuthenticationFindResult authentication_provider_system_find_user_with_password(
    const AuthenticationProvider* auth_provider,
    const char* username, // NOLINT(bugprone-easily-swappable-parameters)
    const char* password) {

	UNUSED(auth_provider);

	// TODO(Totto): https://stackoverflow.com/questions/64184960/pam-authenticate-a-user-in-c
	//  and https://github.com/linux-pam/linux-pam/blob/master/examples/check_user.c

	UNUSED(username);
	UNUSED(password);

	return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
		                               .data = { .error = { .error_message = "TODO" } } };
}

NODISCARD int get_result_value_for_auth_result(AuthenticationFindResult auth) {

	switch(auth.validity) {
		case AuthenticationValidityNoSuchUser: return 1;
		case AuthenticationValidityWrongPassword: return 2;
		case AuthenticationValidityOk: return 3;
		case AuthenticationValidityError:
		default: {
			return 0;
		}
	}
}

NODISCARD int compare_auth_results(AuthenticationFindResult auth1, AuthenticationFindResult auth2) {
	return get_result_value_for_auth_result(auth1) - get_result_value_for_auth_result(auth2);
}

NODISCARD AuthenticationFindResult authentication_providers_find_user_with_password(
    const AuthenticationProviders* auth_providers,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password) {

	STBDS_ARRAY(AuthenticationFindResult) results = STBDS_ARRAY_EMPTY;

	for(size_t i = 0; i < stbds_arrlenu(auth_providers->providers); ++i) {
		AuthenticationProvider* provider = auth_providers->providers[i];

		AuthenticationFindResult result;

		switch(provider->type) {
			case AuthenticationProviderTypeSimple: {
#ifndef _SIMPLE_SERVER_USE_BCRYPT
				result = (AuthenticationFindResult){
					.validity = AuthenticationValidityError,
					.data = { .error = { .error_message = "not compiled with support for provider "
					                                      "type simple" } }
				};
#else
				result = authentication_provider_simple_find_user_with_password(provider, username,
				                                                                password);
#endif
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

		// note: the clang analyzer is incoreect here, we return a item, that is malloced, but we
		// free it everywhere, we use this function!
		if(result.validity == AuthenticationValidityOk) { // NOLINT(clang-analyzer-unix.Malloc)
			stbds_arrfree(results);
			return result;
		}

		if(result.validity == AuthenticationValidityError) {
			LOG_MESSAGE(LogLevelTrace, "Error in account find user, provider %s: %s\n",
			            get_name_for_auth_provider_type(provider->type),
			            result.data.error.error_message);
		}

		stbds_arrput(results, result);
	}

	size_t results_length = stbds_arrlenu(results);

	AuthenticationFindResult best_result = (AuthenticationFindResult){
		.validity = AuthenticationValidityError,
		.data = { .error = { .error_message = "no single provider registered" } }
	};

	for(size_t i = 0; i < results_length; ++i) {
		AuthenticationFindResult current_result = results[i];

		if(compare_auth_results(best_result, current_result) <= 0) {
			best_result = current_result;
		}
	}

	stbds_arrfree(results);

	return best_result;
}
