
#include "./account.h"
#include "utils/log.h"

#include <stdlib.h>

AccountInfo* alloc_default_account(void) {
	AccountInfo* account = (AccountInfo*)malloc(sizeof(AccountInfo));

	if(!account) {
		return NULL;
	}

	*account = new_account_info_empty();

	return account;
}

void free_account_data(AccountInfo* account) {
	SWITCH_ACCOUNT_INFO((*account)) {
		CASE_ACCOUNT_INFO_IS_OK_MUT(*account) {
			tstr_free(&(ok.username));
		}
		break;
		CASE_ACCOUNT_INFO_IS_ONLY_USER_MUT(*account) {
			tstr_free(&(only_user.username));
		}
		break;
		CASE_ACCOUNT_INFO_IS_EMPTY_CONST() {}
		default: {
			break;
		}
	}
}

UserValidity
account_verify(const AuthenticationProviders* auth_providers,
               const tstr* const username, // NOLINT(bugprone-easily-swappable-parameters)
               const tstr* const password) {

	const AuthenticationFindResult result =
	    authentication_providers_find_user_with_password(auth_providers, username, password);

	switch(result.validity) {
		case AuthenticationValidityError: {
			LOG_MESSAGE(LogLevelError,
			            "An error occurred, while trying to find a user with password: %s\n",
			            result.data.error.error_message);
			return UserValidityInternalError;
		}
		case AuthenticationValidityNoSuchUser: {
			return UserValidityNoSuchUser;
		}
		case AuthenticationValidityWrongPassword: {
			return UserValidityWrongPassword;
		}
		case AuthenticationValidityOk: {
			return UserValidityOk;
		}
		default: {
			LOG_MESSAGE_SIMPLE(LogLevelError, "An error occurred, while trying to find a user with "
			                                  "password, unexpected return type enum value\n");

			return UserValidityInternalError;
		}
	}
}
