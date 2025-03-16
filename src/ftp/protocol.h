

#pragma once

#include "utils/utils.h"

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	// INTERNAL USAGE ONLY
	INTERNAL_FTP_RETURN_CODE_MINIMUM = 100,
	INTERNAL_FTP_RETURN_CODE_MAXIMUM = 999,
	//
	// 1xx
	FTP_RETURN_CODE_DATA_CONNECTION_ALREADY_OPEN = 125,
	FTP_RETURN_CODE_DATA_CONNECTION_WAITING_FOR_OPEN = 150,
	// 2xx
	FTP_RETURN_CODE_CMD_OK = 200,
	FTP_RETURN_CODE_OPTIONAL_COMMAND_NOT_IMPLEMENTED = 202,
	FTP_RETURN_CODE_FEATURE_LIST = 211,
	FTP_RETURN_CODE_SYSTEM_NAME = 215,
	FTP_RETURN_CODE_SRVC_READY = 220,
	FTP_RETURN_CODE_CLOSING_DATA_CONNECTION_REQ_OK = 226,
	FTP_RETURN_CODE_ENTERING_PASSIVE_MODE = 227,
	FTP_RETURN_CODE_USER_LOGGED_IN = 230,
	FTP_RETURN_CODE_FILE_ACTION_OK = 250,
	FTP_RETURN_CODE_DIR_OP_SUCC = 257,
	// 3xx
	FTP_RETURN_CODE_NEED_PSWD = 331,
	// 4xx
	FTP_RETURN_CODE_DATA_CONNECTION_OPEN_ERROR = 425,
	FTP_RETURN_CODE_DATA_CONNECTION_CLOSED = 426,
	FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN = 450,
	FTP_RETURN_CODE_FILE_ACTION_ABORTED = 451,
	FTP_RETURN_CODE_FILE_ACTION_NO_STORAGE = 452,
	// 5xx
	FTP_RETURN_CODE_SYNTAX_ERROR = 500,
	FTP_RETURN_CODE_SYNTAX_ERROR_IN_PARAM = 501,
	FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED = 502,
	FTP_RETURN_CODE_BAD_SEQUENCE = 503,
	FTP_RETURN_CODE_COMMAND_NOT_IMPLEMENTED_FOR_PARAM = 504,
	FTP_RETURN_CODE_NOT_LOGGED_IN = 530,
	FTP_RETURN_CODE_FILE_ACTION_NOT_TAKEN_FATAL = 550,
} FTP_RETURN_CODE;
