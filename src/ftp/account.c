
#include "./account.h"

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

UserValidity
account_verify(const AuthenticationProviders* auth_providers,
               const char* const username, // NOLINT(bugprone-easily-swappable-parameters)
               const char* const passwd) {

	UNUSED(auth_providers);

	// TODO(Totto): https://stackoverflow.com/questions/64184960/pam-authenticate-a-user-in-c
	//  and https://github.com/linux-pam/linux-pam/blob/master/examples/check_user.c

	UNUSED(username);
	UNUSED(passwd);

	return UserValidityInternalError;
}
