#include "./authentication.h"

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT
	#include <bcrypt.h>
#endif

#include "utils/log.h"

#include <tmap.h>
#include <tvec.h>

typedef struct {
	HashSaltResultType* hash_salted_password;
	UserRole role;
} SimpleAccountEntry;

/* NOLINTBEGIN(misc-use-internal-linkage,totto-function-passing-type) */
TMAP_DEFINE_AND_IMPLEMENT_MAP_TYPE(tstr, TSTR_KEYNAME, SimpleAccountEntry,
                                   SimpleAccountEntryHashMap)
/* NOLINTEND(misc-use-internal-linkage,totto-function-passing-type) */

typedef struct {
	TMAP_TYPENAME_MAP(SimpleAccountEntryHashMap) entries;
	HashSaltSettings settings;
} SimpleAuthenticationProviderData;

GENERATE_VARIANT_CORE_AUTHENTICATION_PROVIDER()

//  TODO(Totto): do we really need to store ptrs here
/* NOLINTBEGIN(misc-use-internal-linkage) */
TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE_EXTENDED(AuthenticationProvider*, AuthenticationProviderPtr)
/* NOLINTEND(misc-use-internal-linkage) */

struct AuthenticationProvidersImpl {
	TVEC_TYPENAME(AuthenticationProviderPtr) providers;
};

NODISCARD const char* get_name_for_auth_provider_type(const AuthenticationProviderType type) {
	switch(type) {
		case AuthenticationProviderTypeSimple: return "simple authentication provider";
		case AuthenticationProviderTypeSystem: return "system authentication provider";
		default: return "<unknown>";
	}
}

NODISCARD const char* get_name_for_user_role(const UserRole role) {
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

	auth_providers->providers = TVEC_EMPTY(AuthenticationProviderPtr);

	return auth_providers;
}

NODISCARD AuthenticationProvider* initialize_simple_authentication_provider(void) {

#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
	return NULL;
#else

	AuthenticationProvider* auth_provider = malloc(sizeof(AuthenticationProvider));

	if(!auth_provider) {
		return NULL;
	}

	*auth_provider = new_authentication_provider_simple(
	    (SimpleAuthenticationProviderData){ .entries = TMAP_INIT(SimpleAccountEntryHashMap),
	                                        .settings = {
	                                            .work_factor = BCRYPT_DEFAULT_WORK_FACTOR,
	                                            .use_sha512 = true,
	                                        } });

	return auth_provider;
#endif
}

NODISCARD AuthenticationProvider* initialize_system_authentication_provider(void) {
	AuthenticationProvider* auth_provider = malloc(sizeof(AuthenticationProvider));

	if(!auth_provider) {
		return NULL;
	}

	*auth_provider = new_authentication_provider_system();

	return auth_provider;
}

NODISCARD bool add_authentication_provider(AuthenticationProviders* const auth_providers,
                                           AuthenticationProvider* const provider) {

	if(!provider) {
		return false;
	}

	const TvecResult zvec_result =
	    TVEC_PUSH(AuthenticationProviderPtr, &auth_providers->providers, provider);

	return zvec_result == TvecResultOk; // NOLINT(readability-implicit-bool-conversion)
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_raw(
    AuthenticationProvider* const simple_authentication_provider,
    const tstr* const username, // NOLINT(bugprone-easily-swappable-parameters)
    const tstr* const password, const UserRole role) {

#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
	UNUSED(simple_authentication_provider);
	UNUSED(username);
	UNUSED(password);
	UNUSED(role);
	return false;
#else

	IF_AUTHENTICATION_PROVIDER_IS_NOT_SIMPLE(*simple_authentication_provider) {
		return false;
	}

	const SimpleAuthenticationProviderData* const data =
	    authentication_provider_get_as_simple_const_ref(simple_authentication_provider);

	// Note: this may take some time, as bcrypt takes some time, depending on work factor
	HashSaltResultType* hash_salted_password =
	    hash_salt_string(data->settings, tstr_cstr(password));

	return add_user_to_simple_authentication_provider_data_password_hash_salted(
	    simple_authentication_provider, username, hash_salted_password, role);
#endif
}

NODISCARD bool add_user_to_simple_authentication_provider_data_password_hash_salted(
    AuthenticationProvider* const simple_authentication_provider, const tstr* const username,
    HashSaltResultType* const hash_salted_password, const UserRole role) {

	IF_AUTHENTICATION_PROVIDER_IS_NOT_SIMPLE(*simple_authentication_provider) {
		return false;
	}

	SimpleAuthenticationProviderData* const data =
	    authentication_provider_get_as_simple_mut_ref(simple_authentication_provider);

	const SimpleAccountEntry entry = { .hash_salted_password = hash_salted_password, .role = role };

	tstr username_dup = tstr_dup(username);

	const TmapInsertResult insert_result =
	    TMAP_INSERT(SimpleAccountEntryHashMap, &(data->entries), username_dup, entry, false);

	if(insert_result != TmapInsertResultOk) {
		tstr_free(&username_dup);
		return false;
	}

	return true;
}

static void free_simple_authentication_provider(SimpleAuthenticationProviderData* const data) {

#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
	UNUSED(data);
#else

	TMAP_TYPENAME_ITER(SimpleAccountEntryHashMap)
	iter = TMAP_ITER_INIT(SimpleAccountEntryHashMap, &(data->entries));

	TMAP_TYPENAME_ENTRY(SimpleAccountEntryHashMap) value;

	while(TMAP_ITER_NEXT(SimpleAccountEntryHashMap, &iter, &value)) {
		free_hash_salted_result(value.value.hash_salted_password);
		tstr_free(&value.key);
	}

	TMAP_FREE(SimpleAccountEntryHashMap, &(data->entries));

#endif
}

void free_authentication_provider(AuthenticationProvider* const auth_provider) {
	SWITCH_AUTHENTICATION_PROVIDER(*auth_provider) {
		CASE_AUTHENTICATION_PROVIDER_IS_SIMPLE_MUT(*auth_provider) {
			free_simple_authentication_provider(&simple);
			free(auth_provider);
		}
		break;
		CASE_AUTHENTICATION_PROVIDER_IS_SYSTEM() {
			free(auth_provider);
		}
		break;
		default: {
			break;
		}
	}
}

void free_authentication_providers(AuthenticationProviders* const auth_providers) {

	for(size_t i = 0; i < TVEC_LENGTH(AuthenticationProviderPtr, auth_providers->providers); ++i) {
		AuthenticationProvider* const* const auth_provider =
		    TVEC_GET_AT_MUT(AuthenticationProviderPtr, &(auth_providers->providers), i);
		free_authentication_provider(*auth_provider);
	}

	TVEC_FREE(AuthenticationProviderPtr, &(auth_providers->providers));

	free(auth_providers);
}

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT

NODISCARD static const TMAP_TYPENAME_ENTRY(SimpleAccountEntryHashMap) *
    find_user_by_name_simple(const SimpleAuthenticationProviderData* const data,
                             const tstr* const username) {

	if(TMAP_IS_EMPTY(SimpleAccountEntryHashMap, &(data->entries))) {
		return NULL;
	}

	const TMAP_TYPENAME_ENTRY(SimpleAccountEntryHashMap)* entry =
	    TMAP_GET_ENTRY(SimpleAccountEntryHashMap, &(data->entries), *username);

	if(entry == NULL) {
		return NULL;
	}

	return entry;
}

NODISCARD static AuthenticationFindResult authentication_provider_simple_find_user_with_password(
    const AuthenticationProvider* const auth_provider,
    const tstr* const username, // NOLINT(bugprone-easily-swappable-parameters)
    const tstr* const password) {

	IF_AUTHENTICATION_PROVIDER_IS_NOT_SIMPLE(*auth_provider) {

		return new_authentication_find_result_error("Implementation error");
	}

	const SimpleAuthenticationProviderData* const data =
	    authentication_provider_get_as_simple_const_ref(auth_provider);

	const TMAP_TYPENAME_ENTRY(SimpleAccountEntryHashMap)* entry =
	    find_user_by_name_simple(data, username);

	if(!entry) {
		return new_authentication_find_result_no_such_user();
	}

	const bool is_valid_pw = is_string_equal_to_hash_salted_string(
	    data->settings, tstr_cstr(password), entry->value.hash_salted_password);

	if(!is_valid_pw) {
		return new_authentication_find_result_wrong_password();
	}

	// TODO(Totto): maybe don't allocate this?
	const AuthUser user = { .username = tstr_dup(&(entry->key)), .role = entry->value.role };

	return new_authentication_find_result_ok(
	    (AuthUserWithContext){ .provider_type = AuthenticationProviderTypeSimple, .user = user });
}

#endif

#if defined(__linux__)

	#include <pwd.h>
	#include <sys/types.h>
	#include <unistd.h>

	#define INITIAL_SIZE_FOR_LINUX_FUNCS 0xFF

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	LinuxUserResponseError = 0,
	LinuxUserResponseOk,
	LinuxUserResponseNoSuchUser,
} LinuxUserResponse;

NODISCARD MAYBE_UNUSED static LinuxUserResponse check_for_user_linux(const char* const username,
                                                                     gid_t* const group_id) {

	struct passwd result = {};

	struct passwd* result_ptr = NULL;

	SizedBuffer buffer = { .data = NULL, .size = 0 };

	long initial_size = // NOLINT(totto-use-fixed-width-types-var)
	    sysconf(_SC_GETPW_R_SIZE_MAX);

	if(initial_size < 0) {
		initial_size = INITIAL_SIZE_FOR_LINUX_FUNCS;
	}

	buffer.data = malloc(initial_size);

	if(!buffer.data) {
		return LinuxUserResponseError;
	}

	buffer.size = initial_size;

	while(true) {

		const int res = // NOLINT(totto-use-fixed-width-types-var)
		    getpwnam_r(username, &result, buffer.data, buffer.size, &result_ptr);

		if(res == 0) {
			free_sized_buffer(buffer);
			if(result_ptr == NULL) {
				return LinuxUserResponseNoSuchUser;
			}

			*group_id = result_ptr->pw_gid;
			return LinuxUserResponseOk;
		}

		if(res == ERANGE) {
			buffer.size = buffer.size * 2;
			void* new_data = realloc(buffer.data, buffer.size);
			if(!new_data) {
				free(buffer.data); // not calling free_sized_buffer, as the size is invalid, and if
				                   // we in the future might use free_sized with own memory
				                   // allocator, it could go wrong
				return LinuxUserResponseError;
			}

			buffer.data = new_data;
			continue;
		}

		free_sized_buffer(buffer);
		return LinuxUserResponseError;
	}
}

	#include <grp.h>

NODISCARD static char* get_group_name(const gid_t group_id) {

	struct group result = {};

	struct group* result_ptr = NULL;

	SizedBuffer buffer = { .data = NULL, .size = 0 };

	long initial_size = // NOLINT(totto-use-fixed-width-types-var)
	    sysconf(_SC_GETGR_R_SIZE_MAX);

	if(initial_size < 0) {
		initial_size = INITIAL_SIZE_FOR_LINUX_FUNCS;
	}

	buffer.data = malloc(initial_size);

	if(!buffer.data) {
		return NULL;
	}

	buffer.size = initial_size;

	while(true) {

		const int res = // NOLINT(totto-use-fixed-width-types-var)
		    getgrgid_r(group_id, &result, buffer.data, buffer.size, &result_ptr);

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

NODISCARD MAYBE_UNUSED static UserRole get_role_for_linux_user(const char* const username,
                                                               const gid_t group_id) {

	int ngroups = 0; // NOLINT(totto-use-fixed-width-types-var)

	int res = // NOLINT(totto-use-fixed-width-types-var)
	    getgrouplist(username, group_id, NULL, &ngroups);

	if(res != -1) {
		return UserRoleNone;
	}

	gid_t* group_ids = malloc(sizeof(gid_t) * ngroups);

	res = getgrouplist(username, ngroups, group_ids, &ngroups);

	if(res < 0 || ngroups < 0) {
		free(group_ids);
		return UserRoleNone;
	}

	// map group names to roles, just using some common names like root to map to common roles, e.g.
	// admin etc
	UserRole role = UserRoleNone;

	for(size_t i = 0; i < (size_t)ngroups; i++) {
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
NODISCARD static int // NOLINT(totto-use-fixed-width-types-var)
pam_conversation_for_password(
    const int num_msg,                     // NOLINT(totto-use-fixed-width-types-var)
    const struct pam_message** const msgs, // NOLINT(totto-const-correctness-c)
    struct pam_response** const resp, ANY_TYPE(PamAppdata) const appdata_ptr) {

	const PamAppdata* const appdata = (PamAppdata* const)appdata_ptr;

	if(num_msg <= 0) {
		return PAM_CONV_ERR;
	}

	struct pam_response* reply = (struct pam_response*)calloc(num_msg, sizeof(struct pam_response));
	if(!reply) {
		return PAM_CONV_ERR;
	}

	for(size_t i = 0; i < (size_t)num_msg; ++i) {
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
NODISCARD static PamUserResponse pam_is_user_password_combo_ok(
    const char* const username, // NOLINT(bugprone-easily-swappable-parameters)
    const char* const password) {

	pam_handle_t* pamh = NULL;

	PamAppdata app_data = { .password = password };

	const struct pam_conv conv = { .conv = pam_conversation_for_password,
		                           .appdata_ptr = &app_data };

	int res = // NOLINT(totto-use-fixed-width-types-var)
	    pam_start("check_user", username, &conv, &pamh);

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

	const int pam_end_res = pam_end(pamh, res); // NOLINT(totto-use-fixed-width-types-var)

	if(pam_end_res != PAM_SUCCESS) {
		return PamUserResponseError;
	}

	return response;
}

	#endif

NODISCARD static AuthenticationFindResult
authentication_provider_system_find_user_with_password_linux(
    const tstr* const username, // NOLINT(bugprone-easily-swappable-parameters)
    const tstr* const password) {

	#ifndef _SIMPLE_SERVER_HAVE_PAM
	UNUSED(username);
	UNUSED(password);
	return (AuthenticationFindResult){
		.validity = AuthenticationValidityError,
		.data = { .error = { .error_message = "not compiled with pam, not possible to do" } }
	};
	#else

	gid_t group_id = 0;

	const LinuxUserResponse user_response = check_for_user_linux(tstr_cstr(username), &group_id);

	switch(user_response) {
		case LinuxUserResponseNoSuchUser: {
			return new_authentication_find_result_no_such_user();
		}
		case LinuxUserResponseOk: {
			break;
		}
		case LinuxUserResponseError:
		default: {
			return new_authentication_find_result_error("couldn't fetch user information");
		}
	}

	const PamUserResponse pam_response =
	    pam_is_user_password_combo_ok(tstr_cstr(username), tstr_cstr(password));

	switch(pam_response) {

		case PamUserResponseNoSuchUser: {
			return new_authentication_find_result_no_such_user();
		}
		case PamUserResponseOk: {
			break;
		}
		case PamUserResponseError: {
			return new_authentication_find_result_wrong_password();
		}
		default: {
			return new_authentication_find_result_error("pam checking failed");
		}
	}

	const UserRole role = get_role_for_linux_user(tstr_cstr(username), group_id);

	// TODO(Totto): maybe don't allocate this?
	const AuthUser user = { .username = tstr_dup(username), .role = role };

	return new_authentication_find_result_ok(
	    (AuthUserWithContext){ .provider_type = AuthenticationProviderTypeSystem, .user = user });

	#endif
}

#endif

NODISCARD static AuthenticationFindResult authentication_provider_system_find_user_with_password(
    const AuthenticationProvider* const auth_provider,
    const tstr* const username, // NOLINT(bugprone-easily-swappable-parameters)
    const tstr* const password) {

	IF_AUTHENTICATION_PROVIDER_IS_NOT_SYSTEM(*auth_provider) {

		return new_authentication_find_result_error("Implementation error");
	}

#if defined(__linux__)
	return authentication_provider_system_find_user_with_password_linux(username, password);
#else
	UNUSED(username);
	UNUSED(password);
	return new_authentication_find_result_error("not implementzed for this OS");
#endif
}

NODISCARD static int8_t get_result_value_for_auth_result(const AuthenticationFindResult auth) {

	SWITCH_AUTHENTICATION_FIND_RESULT(auth) {
		CASE_AUTHENTICATION_FIND_RESULT_IS_NO_SUCH_USER() {
			return 1;
		}
		CASE_AUTHENTICATION_FIND_RESULT_IS_WRONG_PASSWORD() {
			return 2;
		}
		CASE_AUTHENTICATION_FIND_RESULT_IS_OK_IGN() {
			return 3;
		}
		CASE_AUTHENTICATION_FIND_RESULT_IS_ERROR_IGN() {
			return 0;
		}
		default: {
			return 0;
		}
	}
}

NODISCARD static int8_t compare_auth_results(const AuthenticationFindResult auth1,
                                             const AuthenticationFindResult auth2) {
	return (int8_t)(get_result_value_for_auth_result(auth1) -
	                get_result_value_for_auth_result(auth2));
}
/* NOLINTBEGIN(misc-use-internal-linkage) */
TVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(AuthenticationFindResult)
/* NOLINTEND(misc-use-internal-linkage) */

NODISCARD AuthenticationFindResult authentication_providers_find_user_with_password(
    const AuthenticationProviders* const auth_providers,
    const tstr* const username, // NOLINT(bugprone-easily-swappable-parameters)
    const tstr* const password) {

	TVEC_TYPENAME(AuthenticationFindResult) results = TVEC_EMPTY(AuthenticationFindResult);

	for(size_t i = 0; i < TVEC_LENGTH(AuthenticationProviderPtr, auth_providers->providers); ++i) {
		const AuthenticationProvider* const provider =
		    TVEC_AT(AuthenticationProviderPtr, auth_providers->providers, i);

		AuthenticationFindResult result;

		SWITCH_AUTHENTICATION_PROVIDER(*provider) {
			CASE_AUTHENTICATION_PROVIDER_IS_SIMPLE_IGN() {
#ifndef _SIMPLE_SERVER_HAVE_BCRYPT
				result =
				    new_authentication_find_result_error("not compiled with support for provider "
				                                         "type simple");
#else
				result = authentication_provider_simple_find_user_with_password(provider, username,
				                                                                password);
#endif
			}
			break;
			CASE_AUTHENTICATION_PROVIDER_IS_SYSTEM() {
				result = authentication_provider_system_find_user_with_password(provider, username,
				                                                                password);
				break;
			}
			default: {
				result = new_authentication_find_result_error("unrecognized provider type");
				break;
			}
		}

		// note: the clang analyzer is incoreect here, we return a item, that is malloced, but
		// we free it everywhere, we use this function!
		IF_AUTHENTICATION_FIND_RESULT_IS_OK_IGN(result) { // NOLINT(clang-analyzer-unix.Malloc)
			TVEC_FREE(AuthenticationFindResult, &results);
			return result;
		}

		IF_AUTHENTICATION_FIND_RESULT_IS_ERROR_CONST(result) {
			LOG_MESSAGE(LogLevelTrace, "Error in account find user, provider %s: %s\n",
			            get_name_for_auth_provider_type(
			                get_current_tag_type_for_authentication_provider(*provider)),
			            error.message);
		}

		// TODO(Totto): remove all auto_ = things, properly return in that cases
		const TvecResult push_res = TVEC_PUSH(AuthenticationFindResult, &results, result);
		OOM_ASSERT(push_res == TvecResultOk, "Vec push error");
	}

	const size_t results_length = TVEC_LENGTH(AuthenticationFindResult, results);

	AuthenticationFindResult best_result =
	    new_authentication_find_result_error("no single provider registered");

	for(size_t i = 0; i < results_length; ++i) {
		const AuthenticationFindResult current_result =
		    TVEC_AT(AuthenticationFindResult, results, i);

		if(compare_auth_results(best_result, current_result) <= 0) {
			best_result = current_result;
		}
	}

	TVEC_FREE(AuthenticationFindResult, &results);

	return best_result;
}
