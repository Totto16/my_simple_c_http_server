#include "./authentication.h"

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT
#include <bcrypt.h>
#endif

#include "utils/log.h"

#include <zmap/zmap.h>
#include <zvec/zvec.h>

typedef struct {
	HashSaltResultType* hash_salted_password;
	UserRole role;
} SimpleAccountEntry;

ZMAP_DEFINE_MAP_TYPE(char*, CHAR_PTR_KEYNAME, SimpleAccountEntry, SimpleAccountEntryHashMap)

typedef struct {
	ZMAP_TYPENAME_MAP(SimpleAccountEntryHashMap) entries;
	HashSaltSettings settings;
} SimpleAuthenticationProviderData;

struct AuthenticationProviderImpl {
	AuthenticationProviderType type;
	union {
		SimpleAuthenticationProviderData simple;
	} data;
};

// TODO: do we really need to store ptrs here
ZVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE_EXTENDED(AuthenticationProvider*, AuthenticationProviderPtr)

struct AuthenticationProvidersImpl {
	ZVEC_TYPENAME(AuthenticationProviderPtr) providers;
};

NODISCARD const char* get_name_for_auth_provider_type(AuthenticationProviderType type) {
	switch(type) {
		case AuthenticationProviderTypeSimple: return "simple authentication provider";
		case AuthenticationProviderTypeSystem: return "system authentication provider";
		default: return "<unknown>";
	}
}

NODISCARD const char* get_name_for_user_role(UserRole role) {
	switch(role) {
		case UserRoleNone: return "None";
		case UserRoleAdmin: return "Admin";
		case UserRoleUser: return "User";
		default: return "<unknown>";
	}
}

NODISCARD AuthenticationProviders* initialize_authentication_providers(void) {
	AuthenticationProviders* auth_providers = malloc(sizeof(AuthenticationProviders));

	if(!auth_providers) {
		return NULL;
	}

	auth_providers->providers = ZVEC_EMPTY(AuthenticationProviderPtr);

	return auth_providers;
}

#define TODO_HASH_SETTINGS 20

NODISCARD AuthenticationProvider* initialize_simple_authentication_provider(void) {

#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
	return NULL;
#else

	AuthenticationProvider* auth_provider = malloc(sizeof(AuthenticationProvider));

	if(!auth_provider) {
		return NULL;
	}

	auth_provider->type = AuthenticationProviderTypeSimple;
	auth_provider->data.simple =
	    (SimpleAuthenticationProviderData){ .entries = ZMAP_INIT(SimpleAccountEntryHashMap),
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

	return auth_provider;
}

NODISCARD bool add_authentication_provider(AuthenticationProviders* auth_providers,
                                           AuthenticationProvider* provider) {

	if(!provider) {
		return false;
	}

	ZvecResult zvec_result = ZVEC_PUSH_EXTENDED(AuthenticationProvider, AuthenticationProviderPtr,
	                                            &auth_providers->providers, provider);

	if(zvec_result != ZvecResultOk) {
		return false;
	}
	return true;
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* simple_authentication_provider,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password, UserRole role) {

#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
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
    HashSaltResultType* hash_salted_password, UserRole role) {

	if(simple_authentication_provider->type != AuthenticationProviderTypeSimple) {
		return false;
	}

	SimpleAuthenticationProviderData* data = &simple_authentication_provider->data.simple;

	SimpleAccountEntry entry = { .hash_salted_password = hash_salted_password, .role = role };

	ZmapInsertResult insert_result =
	    ZMAP_INSERT(SimpleAccountEntryHashMap, &(data->entries), strdup(username), entry, false);

	if(insert_result != ZmapInsertResultOk) {
		return false;
	}

	return true;
}

static void free_simple_authentication_provider(SimpleAuthenticationProviderData data) {

#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
	UNUSED(data);
#else

	size_t hm_total_length = ZMAP_CAPACITY(data.entries);

	for(size_t i = 0; i < hm_total_length; ++i) {
		const ZMAP_TYPENAME_ENTRY(SimpleAccountEntryHashMap)* hm_entry =
		    ZMAP_GET_ENTRY_AT(SimpleAccountEntryHashMap, &data.entries, i);

		if(hm_entry != NULL && hm_entry != ZMAP_NO_ELEMENT_HERE) {
			free_hash_salted_result(hm_entry->value.hash_salted_password);
			free(hm_entry->key);
		}
	}

	ZMAP_FREE(SimpleAccountEntryHashMap, &data.entries);

#endif
}

void free_authentication_provider(AuthenticationProvider* auth_provider) {
	switch(auth_provider->type) {
		case AuthenticationProviderTypeSimple: {
			free_simple_authentication_provider(auth_provider->data.simple);
			free(auth_provider);
			break;
		}
		case AuthenticationProviderTypeSystem: {
			free(auth_provider);
			break;
		}
		default: break;
	}
}

void free_authentication_providers(AuthenticationProviders* auth_providers) {

	for(size_t i = 0; i < ZVEC_LENGTH(auth_providers->providers); ++i) {
		AuthenticationProvider** auth_provider = ZVEC_GET_AT_MUT_EXTENDED(
		    AuthenticationProvider*, AuthenticationProviderPtr, &(auth_providers->providers), i);
		free_authentication_provider(*auth_provider);
	}

	ZVEC_FREE(AuthenticationProviderPtr, &(auth_providers->providers));

	free(auth_providers);
}

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT

NODISCARD static const SimpleAccountEntry*
find_user_by_name_simple(SimpleAuthenticationProviderData* data, char* username) {

	if(ZMAP_IS_EMPTY(data->entries)) {
		// note: if hash_map is NULL stbds_shgeti allocates a new value, that is never populated to
		// the original SimpleAccountEntry value, as this is a struct copy!
		return NULL;
	}

	const SimpleAccountEntry* entry =
	    ZMAP_GET(SimpleAccountEntryHashMap, &(data->entries), username);

	if(entry == NULL) {
		return NULL;
	}

	return entry;
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

	const SimpleAccountEntry* entry = find_user_by_name_simple(data, username);

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
	AuthUser user = { .username = strdup(username), .role = entry->role };

	return (AuthenticationFindResult){
		.validity = AuthenticationValidityOk,
		.data = { .ok = { .provider_type = AuthenticationProviderTypeSimple, .user = user } }
	};
}

#endif

#if defined(__linux__)

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define INITIAL_SIZE_FOR_LINUX_FUNCS 0xFF

NODISCARD MAYBE_UNUSED static int check_for_user_linux(const char* username, gid_t* group_id) {

	struct passwd result = {};

	struct passwd* result_ptr = NULL;

	SizedBuffer buffer = { .data = NULL, .size = 0 };

	long initial_size = sysconf(_SC_GETPW_R_SIZE_MAX);

	if(initial_size < 0) {
		initial_size = INITIAL_SIZE_FOR_LINUX_FUNCS;
	}

	buffer.data = malloc(initial_size);

	if(!buffer.data) {
		return -1;
	}

	buffer.size = initial_size;

	while(true) {

		int res = getpwnam_r(username, &result, buffer.data, buffer.size, &result_ptr);

		if(res == 0) {
			free_sized_buffer(buffer);
			if(result_ptr == NULL) {
				return 0;
			}

			*group_id = result_ptr->pw_gid;
			return 1;
		}

		if(res == ERANGE) {
			buffer.size = buffer.size * 2;
			void* new_data = realloc(buffer.data, buffer.size);
			if(!new_data) {
				free(buffer.data); // not calling free_sized_buffer, as the size is invalid, and if
				                   // we in the future might use free_sized with own memory
				                   // allocator, it could go wrong
				return -1;
			}

			buffer.data = new_data;
			continue;
		}

		free_sized_buffer(buffer);
		return -1;
	}
}

#include <grp.h>

NODISCARD static char* get_group_name(gid_t group_id) {

	struct group result = {};

	struct group* result_ptr = NULL;

	SizedBuffer buffer = { .data = NULL, .size = 0 };

	long initial_size = sysconf(_SC_GETGR_R_SIZE_MAX);

	if(initial_size < 0) {
		initial_size = INITIAL_SIZE_FOR_LINUX_FUNCS;
	}

	buffer.data = malloc(initial_size);

	if(!buffer.data) {
		return NULL;
	}

	buffer.size = initial_size;

	while(true) {

		int res = getgrgid_r(group_id, &result, buffer.data, buffer.size, &result_ptr);

		if(res == 0) {
			if(result_ptr == NULL) {
				free_sized_buffer(buffer);
				return NULL;
			}

			char* group_name = strdup(result_ptr->gr_name);
			free_sized_buffer(buffer);
			return group_name;
		}

		if(res == ERANGE) {
			buffer.size = buffer.size * 2;
			void* new_data = realloc(buffer.data, buffer.size);
			if(!new_data) {
				free(buffer.data); // not calling free_sized_buffer, as the size is invalid, and if
				                   // we in the future might use free_sized with own memory
				                   // allocator, it could go wrong
				return NULL;
			}

			buffer.data = new_data;
			continue;
		}

		free_sized_buffer(buffer);
		return NULL;
	}
}

NODISCARD MAYBE_UNUSED static UserRole get_role_for_linux_user(const char* username,
                                                               gid_t group_id) {

	int ngroups = 0;

	int res = getgrouplist(username, group_id, NULL, &ngroups);

	if(res != -1) {
		return UserRoleNone;
	}

	gid_t* group_ids = malloc(sizeof(gid_t) * ngroups);

	res = getgrouplist(username, ngroups, group_ids, &ngroups);

	if(res < 0) {
		free(group_ids);
		return UserRoleNone;
	}

	// map group names to roles, just using some common names like root to map to common roles, e.g.
	// admin etc
	UserRole role = UserRoleNone;

	for(int i = 0; i < ngroups; i++) {
		char* name = get_group_name(group_ids[i]);
		if(name == NULL) {
			continue;
		}

		if(strcmp(name, "root") == 0) {
			free(name);
			role = UserRoleAdmin;
			break;
		}

		if(strcmp(name, "sudo") == 0) {
			free(name);
			role = UserRoleAdmin;
			break;
		}

		if(strcmp(name, "wheel") == 0) {
			free(name);
			role = UserRoleAdmin;
			break;
		}

		if(strcmp(name, username) == 0) {
			free(name);
			role = UserRoleUser;
			continue;
		}

		free(name);
	}

	free(group_ids);
	return role;
}

#ifdef _SIMPLE_SERVER_HAVE_PAM

#include <security/pam_appl.h>
#include <security/pam_misc.h>

typedef struct {
	const char* password;
} PamAppdata;

// based on
// https://github.com/linux-pam/linux-pam/blob/e3b66a60e4209e019cf6a45f521858cec2dbefa1/libpam_misc/misc_conv.c#L280
NODISCARD static int pam_conversation_for_password(int num_msg, const struct pam_message** msgs,
                                                   struct pam_response** resp,
                                                   ANY_TYPE(PamAppdata) appdata_ptr) {

	PamAppdata* appdata = (PamAppdata*)appdata_ptr;

	if(num_msg <= 0) {
		return PAM_CONV_ERR;
	}

	struct pam_response* reply = (struct pam_response*)calloc(num_msg, sizeof(struct pam_response));
	if(!reply) {
		return PAM_CONV_ERR;
	}

	for(int i = 0; i < num_msg; ++i) {
		const struct pam_message* msg = msgs[i];

		switch(msg->msg_style) {
			case PAM_PROMPT_ECHO_OFF:
			case PAM_PROMPT_ECHO_ON: {
				reply[i].resp = strdup(appdata->password);
				reply[i].resp_retcode = 0;
				break;
			}
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
			case PAM_RADIO_TYPE:
			case PAM_BINARY_PROMPT: {
				reply[i].resp = NULL;
				reply[i].resp_retcode = 0;
				break;
			}
			default: {
				free(reply);
				*resp = NULL;
				return PAM_CONV_ERR;
			}
		}
	}

	*resp = reply;
	return PAM_SUCCESS;
}

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	PamUserResponseError = 0,
	PamUserResponseOk,
	PamUserResponseNoSuchUser,
} PamUserResponse;

// see https://stackoverflow.com/questions/64184960/pam-authenticate-a-user-in-c
// and https://github.com/linux-pam/linux-pam/blob/master/examples/check_user.c
NODISCARD static PamUserResponse
pam_is_user_password_combo_ok(const char* username, // NOLINT(bugprone-easily-swappable-parameters)
                              const char* password) {

	pam_handle_t* pamh = NULL;

	PamAppdata app__data = { .password = password };

	struct pam_conv conv = { .conv = pam_conversation_for_password, .appdata_ptr = &app__data };

	int res = pam_start("check_user", username, &conv, &pamh);

	if(res != PAM_SUCCESS) {

		LOG_MESSAGE(LogLevelError, "pam_start failed: %s\n", pam_strerror(pamh, res));
		return PamUserResponseError;
	}

	res = pam_authenticate(pamh, 0);

	PamUserResponse response = PamUserResponseError;

	switch(res) {
		case PAM_SUCCESS: {
			response = PamUserResponseOk;
			break;
		}
		case PAM_USER_UNKNOWN: {
			response = PamUserResponseNoSuchUser;
			break;
		}
		case PAM_AUTH_ERR:
		case PAM_ABORT:
		case PAM_CRED_INSUFFICIENT:
		case PAM_AUTHINFO_UNAVAIL:
		case PAM_MAXTRIES:
		default: {
			response = PamUserResponseError;
			break;
		}
	}

	int pam_end_res = pam_end(pamh, res);

	if(pam_end_res != PAM_SUCCESS) {
		return PamUserResponseError;
	}

	return response;
}

#endif

NODISCARD static AuthenticationFindResult
authentication_provider_system_find_user_with_password_linux(
    const char* username, // NOLINT(bugprone-easily-swappable-parameters)
    const char* password) {

#ifndef _SIMPLE_SERVER_HAVE_PAM
	UNUSED(username);
	UNUSED(password);
	return (AuthenticationFindResult){
		.validity = AuthenticationValidityError,
		.data = { .error = { .error_message = "not compiled with pam, not possible to do" } }
	};
#else

	gid_t group_id = 0;

	int does_user_exist = check_for_user_linux(username, &group_id);

	if(does_user_exist < 0) {
		return (AuthenticationFindResult){
			.validity = AuthenticationValidityError,
			.data = { .error = { .error_message = "couldn'T fetch user information" } }
		};
	}

	if(!does_user_exist) {
		return (AuthenticationFindResult){ .validity = AuthenticationValidityNoSuchUser,
			                               .data = {} };
	}

	int password_matches = pam_is_user_password_combo_ok(username, password);

	if(password_matches < 0) {
		return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
			                               .data = { .error = { .error_message =
			                                                        "pam checking failed" } } };
	}

	if(!password_matches) {
		return (AuthenticationFindResult){ .validity = AuthenticationValidityWrongPassword,
			                               .data = {} };
	}

	UserRole role = get_role_for_linux_user(username, group_id);

	// TODO(Totto): maybe don't allocate this?
	AuthUser user = { .username = strdup(username), .role = role };

	return (AuthenticationFindResult){
		.validity = AuthenticationValidityOk,
		.data = { .ok = { .provider_type = AuthenticationProviderTypeSystem, .user = user } }
	};

#endif
}

#endif

NODISCARD static AuthenticationFindResult authentication_provider_system_find_user_with_password(
    const AuthenticationProvider* auth_provider,
    const char* username, // NOLINT(bugprone-easily-swappable-parameters)
    const char* password) {

	if(auth_provider->type != AuthenticationProviderTypeSystem) {

		return (AuthenticationFindResult){ .validity = AuthenticationValidityError,
			                               .data = { .error = { .error_message =
			                                                        "Implementation error" } } };
	}

#if defined(__linux__)
	return authentication_provider_system_find_user_with_password_linux(username, password);
#else
	UNUSED(username);
	UNUSED(password);
	return (AuthenticationFindResult){
		.validity = AuthenticationValidityError,
		.data = { .error = { .error_message = "not implementzed for this OS" } }
	};

#endif
}

NODISCARD static int get_result_value_for_auth_result(AuthenticationFindResult auth) {

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

NODISCARD static int compare_auth_results(AuthenticationFindResult auth1,
                                          AuthenticationFindResult auth2) {
	return get_result_value_for_auth_result(auth1) - get_result_value_for_auth_result(auth2);
}

ZVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(AuthenticationFindResult)

NODISCARD AuthenticationFindResult authentication_providers_find_user_with_password(
    const AuthenticationProviders* auth_providers,
    char* username, // NOLINT(bugprone-easily-swappable-parameters)
    char* password) {

	ZVEC_TYPENAME(AuthenticationFindResult) results = ZVEC_EMPTY(AuthenticationFindResult);

	for(size_t i = 0; i < ZVEC_LENGTH(auth_providers->providers); ++i) {
		AuthenticationProvider* provider =
		    ZVEC_AT(AuthenticationProviderPtr, auth_providers->providers, i);

		AuthenticationFindResult result;

		switch(provider->type) {
			case AuthenticationProviderTypeSimple: {
#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
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

		// note: the clang analyzer is incoreect here, we return a item, that is malloced, but
		// we free it everywhere, we use this function!
		if(result.validity == AuthenticationValidityOk) { // NOLINT(clang-analyzer-unix.Malloc)
			ZVEC_FREE(AuthenticationFindResult, &results);
			return result;
		}

		if(result.validity == AuthenticationValidityError) {
			LOG_MESSAGE(LogLevelTrace, "Error in account find user, provider %s: %s\n",
			            get_name_for_auth_provider_type(provider->type),
			            result.data.error.error_message);
		}

		// TODO
		auto _ = ZVEC_PUSH(AuthenticationFindResult, &results, result);
		UNUSED(_);
	}

	size_t results_length = ZVEC_LENGTH(results);

	AuthenticationFindResult best_result = (AuthenticationFindResult){
		.validity = AuthenticationValidityError,
		.data = { .error = { .error_message = "no single provider registered" } }
	};

	for(size_t i = 0; i < results_length; ++i) {
		AuthenticationFindResult current_result = ZVEC_AT(AuthenticationFindResult, results, i);

		if(compare_auth_results(best_result, current_result) <= 0) {
			best_result = current_result;
		}
	}

	ZVEC_FREE(AuthenticationFindResult, &results);

	return best_result;
}
