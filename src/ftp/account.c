
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
			free(account->data.ok_data.username);
			break;
		}
		case AccountStateOnlyUser: {
			free(account->data.temp_data.username);
			break;
		}
		case AccountStateEmpty:
		default: {
			break;
		}
	}
}

UserValidity account_verify(const AuthenticationProviders* auth_providers,
                            char* username, // NOLINT(bugprone-easily-swappable-parameters)
                            char* password) {

	const AuthenticationFindResult result =
	    authentication_providers_find_user_with_password(auth_providers, username, password);

	switch(result.validity) {
		case AuthenticationValidityError: {
			LOG_MESSAGE(LogLevelError,
			            "An error occured, while trying to find a user with password: %s\n",
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
			LOG_MESSAGE_SIMPLE(LogLevelError, "An error occured, while trying to find a user with "
			                                  "password, unexpected return type enum value\n");

			return UserValidityInternalError;
		}
	}
}
