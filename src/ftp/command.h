

#pragma once

#include <stdlib.h>

#include "./state.h"

/**
 * @ref https://datatracker.ietf.org/doc/html/rfc959  5.3.1
 * @enum value
 */
typedef enum {
	FTP_COMMAND_USER = 0,
	FTP_COMMAND_PASS,
	FTP_COMMAND_ACCT,
	FTP_COMMAND_CWD,
	FTP_COMMAND_CDUP,
	FTP_COMMAND_SMNT,
	FTP_COMMAND_QUIT,
	FTP_COMMAND_REIN,
	FTP_COMMAND_PORT,
	FTP_COMMAND_PASV,
	FTP_COMMAND_TYPE,
	FTP_COMMAND_STRU,
	FTP_COMMAND_MODE,
	FTP_COMMAND_RETR,
	FTP_COMMAND_STOR,
	FTP_COMMAND_STOU,
	FTP_COMMAND_APPE,
	FTP_COMMAND_ALLO,
	FTP_COMMAND_REST,
	FTP_COMMAND_RNFR,
	FTP_COMMAND_RNTO,
	FTP_COMMAND_ABOR,
	FTP_COMMAND_DELE,
	FTP_COMMAND_RMD,
	FTP_COMMAND_MKD,
	FTP_COMMAND_PWD,
	FTP_COMMAND_LIST,
	FTP_COMMAND_NLST,
	FTP_COMMAND_SITE,
	FTP_COMMAND_SYST,
	FTP_COMMAND_STAT,
	FTP_COMMAND_HELP,
	FTP_COMMAND_NOOP,
	// see https://datatracker.ietf.org/doc/html/rfc2228
	// and https://datatracker.ietf.org/doc/html/rfc4217
	FTP_COMMAND_AUTH,
	FTP_COMMAND_ADAT,
	FTP_COMMAND_PROT,
	FTP_COMMAND_PBSZ,
	FTP_COMMAND_MIC,
	FTP_COMMAND_CONF,
	FTP_COMMAND_ENC,
	// see: https://datatracker.ietf.org/doc/html/rfc2389
	FTP_COMMAND_FEAT,
	FTP_COMMAND_OPTS
} FTP_COMMAND_ENUM;

typedef struct {
	bool is_normal;
	union {
		FTP_TRANSMISSION_TYPE type;
		uint8_t num;
	} data;
} FTPCommandTypeInformation;

typedef struct {
	FTP_COMMAND_ENUM type;
	union {
		char* string;
		FTPCommandTypeInformation* type_info;
		FTPPortInformation* port_info;
	} data;

} FTPCommand;

ARRAY_STRUCT(FTPCommandArray, FTPCommand*);

NODISCARD FTPCommandArray* parseMultipleFTPCommands(char* input);

void freeFTPCommand(FTPCommand*);

void freeFTPCommandArray(FTPCommandArray*);

NODISCARD const char* get_command_name(const FTPCommand*);

NODISCARD FTPCommandTypeInformation* parse_ftp_command_type_info(char* arg);
