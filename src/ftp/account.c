
#include "./account.h"
#include "utils/log.h"

#include <stdlib.h>

AccountInfo* alloc_default_account(void) {
	AccountInfo* account = (AccountInfo*)malloc(sizeof(AccountInfo));

	if(!account) {
		return NULL;
	}

	account->state = AccountStateEmpty;

	return account;
}

void free_account_data(AccountInfo* account) {
	switch(account->state) {
		case AccountStateOk: {
			tstr_free(&(account->data.ok_data.username));
			break;
		}
		case AccountStateOnlyUser: {
			tstr_free(&(account->data.temp_data.username));
			break;
		}
		case AccountStateEmpty:
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
