
#include "./account.h"

#include <stdlib.h>

AccountInfo* alloc_default_account(void) {
	AccountInfo* account = (AccountInfo*)malloc(sizeof(AccountInfo));

	if(!account) {
		return NULL;
	}

	account->state = ACCOUNT_STATE_EMPTY;

	return account;
}

void free_account_data(AccountInfo* account) {
	switch(account->state) {
		case ACCOUNT_STATE_OK: {
			free(account->data.ok_data.username);
			break;
		}
		case ACCOUNT_STATE_ONLY_USER: {
			free(account->data.temp_data.username);
			break;
		}
		case ACCOUNT_STATE_EMPTY:
		default: {
			break;
		}
	}
}

USER_VALIDITY account_verify(char* username, // NOLINT(bugprone-easily-swappable-parameters)
                             char* passwd) { // NOLINT(bugprone-easily-swappable-parameters)

	// TODO: https://stackoverflow.com/questions/64184960/pam-authenticate-a-user-in-c
	//  and https://github.com/linux-pam/linux-pam/blob/master/examples/check_user.c

	UNUSED(username);
	UNUSED(passwd);

	return USER_VALIDITY_INTERNAL_ERROR;
}
