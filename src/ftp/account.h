

#pragma once

#include "utils/utils.h"

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ACCOUNT_PERMISSIONS_NONE = 0x00,
	ACCOUNT_PERMISSIONS_READ = 0x01,
	ACCOUNT_PERMISSIONS_WRITE = 0x02,
} ACCOUNT_PERMISSIONS;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ACCOUNT_STATE_EMPTY = 0,
	ACCOUNT_STATE_ONLY_USER,
	ACCOUNT_STATE_OK,
} ACCOUNT_STATE;

typedef struct {
	ACCOUNT_PERMISSIONS permissions;
	char* username;
} AccountOkData;

typedef struct {
	ACCOUNT_STATE state;
	union {
		AccountOkData ok_data;
		struct {
			char* username;
		} temp_data;
	} data;
} AccountInfo;

AccountInfo* alloc_default_account(void);

// TODO free account

void free_account_data(AccountInfo*);

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	USER_VALIDITY_NO_SUCH_USER = 0,
	USER_VALIDITY_WRONG_PASSWORD,
	USER_VALIDITY_OK,
	USER_VALIDITY_INTERNAL_ERROR,
} USER_VALIDITY;

NODISCARD USER_VALIDITY account_verify(char* username, char* passwd);
