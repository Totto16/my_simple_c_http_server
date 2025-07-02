

#pragma once

#include <stdlib.h>

#include "./state.h"
#include <stb/ds.h>

/**
 * @ref https://datatracker.ietf.org/doc/html/rfc959  5.3.1
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FtpCommandUser = 0,
	FtpCommandPass,
	FtpCommandAcct,
	FtpCommandCwd,
	FtpCommandCdup,
	FtpCommandSmnt,
	FtpCommandQuit,
	FtpCommandRein,
	FtpCommandPort,
	FtpCommandPasv,
	FtpCommandType,
	FtpCommandStru,
	FtpCommandMode,
	FtpCommandRetr,
	FtpCommandStor,
	FtpCommandStou,
	FtpCommandAppe,
	FtpCommandAllo,
	FtpCommandRest,
	FtpCommandRnfr,
	FtpCommandRnto,
	FtpCommandAbor,
	FtpCommandDele,
	FtpCommandRmd,
	FtpCommandMkd,
	FtpCommandPwd,
	FtpCommandList,
	FtpCommandNlst,
	FtpCommandSite,
	FtpCommandSyst,
	FtpCommandStat,
	FtpCommandHelp,
	FtpCommandNoop,
	// see https://datatracker.ietf.org/doc/html/rfc2228
	// and https://datatracker.ietf.org/doc/html/rfc4217
	FtpCommandAuth,
	FtpCommandAdat,
	FtpCommandProt,
	FtpCommandPbsz,
	FtpCommandMic,
	FtpCommandConf,
	FtpCommandEnc,
	// see: https://datatracker.ietf.org/doc/html/rfc2389
	FtpCommandFeat,
	FtpCommandOpts
} FtpCommandEnum;

typedef struct {
	bool is_normal;
	union {
		FtpTransmissionType type;
		uint8_t num;
	} data;
} FTPCommandTypeInformation;

typedef struct {
	FtpCommandEnum type;
	union {
		char* string;
		FTPCommandTypeInformation* type_info;
		FTPPortInformation* port_info;
	} data;

} FTPCommand;

typedef STBDS_ARRAY(FTPCommand*) FTPCommandArray;

NODISCARD FTPCommandArray parse_multiple_ftp_commands(char* raw_ftp_commands);

void free_ftp_command(FTPCommand* cmd);

void free_ftp_command_array(FTPCommandArray array);

NODISCARD const char* get_command_name(const FTPCommand* command);

NODISCARD FTPCommandTypeInformation* parse_ftp_command_type_info(char* arg);
