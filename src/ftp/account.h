

#pragma once

#include "generic/authentication.h"
#include "utils/utils.h"
#include <tstr.h>

#include "variants.h"

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	AccountPermissionsNone = 0x00,
	AccountPermissionsRead = 0x01,
	AccountPermissionsWrite = 0x02,
	AccountPermissionsReadWrite = AccountPermissionsRead | AccountPermissionsWrite,
} AccountPermissions;

typedef struct {
	AccountPermissions permissions;
	tstr username;
} AccountOkData;

GENERATE_VARIANT_ALL_ACCOUNT_INFO()

NODISCARD AccountInfo* alloc_default_account(void);

void free_account_data(AccountInfo* account);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	UserValidityNoSuchUser = 0,
	UserValidityWrongPassword,
	UserValidityOk,
	UserValidityInternalError,
} UserValidity;

NODISCARD UserValidity account_verify(const AuthenticationProviders* auth_providers,
                                      const tstr* username, const tstr* password);
