

#pragma once

#include "utils/utils.h"

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	AccountPermissionsNone = 0x00,
	AccountPermissionsRead = 0x01,
	AccountPermissionsWrite = 0x02,
	AccountPermissionsReadWrite = AccountPermissionsRead | AccountPermissionsWrite
} AccountPermissions;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	AccountStateEmpty = 0,
	AccountStateOnlyUser,
	AccountStateOk,
} AccountState;

typedef struct {
	AccountPermissions permissions;
	char* username;
} AccountOkData;

typedef struct {
	AccountState state;
	union {
		AccountOkData ok_data;
		struct {
			char* username;
		} temp_data;
	} data;
} AccountInfo;

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

NODISCARD UserValidity account_verify(const char* username, const char* passwd);
