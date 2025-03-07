

#pragma once

typedef enum {
	ACCOUNT_READ = 1,
	ACCOUNT_WRITE = 2,
	ACCCOUNT_GUEST = 4,
} ACCOUNT_PERMISSIONS;

typedef struct {
	ACCOUNT_PERMISSIONS permissions;
	char* username;
} AccountInfo;
