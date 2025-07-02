

#pragma once

#include "utils/utils.h"

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	// INTERNAL USAGE ONLY
	InternalFtpReturnCodeMinimum = 100,
	InternalFtpReturnCodeMaximum = 999,
	//
	// 1xx
	FtpReturnCodeDataConnectionAlreadyOpen = 125,
	FtpReturnCodeDataConnectionWaitingForOpen = 150,
	// 2xx
	FtpReturnCodeCmdOk = 200,
	FtpReturnCodeOptionalCommandNotImplemented = 202,
	FtpReturnCodeFeatureList = 211,
	FtpReturnCodeSystemName = 215,
	FtpReturnCodeSrvcReady = 220,
	FtpReturnCodeClosingDataConnectionReqOk = 226,
	FtpReturnCodeEnteringPassiveMode = 227,
	FtpReturnCodeUserLoggedIn = 230,
	FtpReturnCodeFileActionOk = 250,
	FtpReturnCodeDirOpSucc = 257,
	// 3xx
	FtpReturnCodeNeedPswd = 331,
	// 4xx
	FtpReturnCodeDataConnectionOpenError = 425,
	FtpReturnCodeDataConnectionClosed = 426,
	FtpReturnCodeFileActionNotTaken = 450,
	FtpReturnCodeFileActionAborted = 451,
	FtpReturnCodeFileActionNoStorage = 452,
	// 5xx
	FtpReturnCodeSyntaxError = 500,
	FtpReturnCodeSyntaxErrorInParam = 501,
	FtpReturnCodeCommandNotImplemented = 502,
	FtpReturnCodeBadSequence = 503,
	FtpReturnCodeCommandNotImplementedForParam = 504,
	FtpReturnCodeNotLoggedIn = 530,
	FtpReturnCodeNedAcctForStore = 532,
	FtpReturnCodeFileActionNotTakenFatal = 550,
} FtpReturnCode;
