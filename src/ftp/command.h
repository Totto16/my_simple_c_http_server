

#pragma once

#include <stdlib.h>

#include "./state.h"
#include "utils/buffered_reader.h"
#include <tstr.h>
#include <tvec.h>

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
	bool has_value;
	tstr value;
} OptionalString;

// command types
#define FTP_COMMAND_TYPE_NONE 0
#define FTP_COMMAND_TYPE_STRING 1
#define FTP_COMMAND_TYPE_OPT_STRING 2
// special types
#define FTP_COMMAND_TYPE_TYPE_INFO 17
#define FTP_COMMAND_TYPE_PORT_INFO 18

// property mappings

#define PROPERTY_VALUE_FOR(val) PROPERTY_VALUE_FOR_IND(val)

#define PROPERTY_VALUE_FOR_IND(val) PROPERTY_VALUE_FOR_IMPL_##val

#define PROPERTY_VALUE_FOR_IMPL_0 static_assert(false, "doesn't have a property")
#define PROPERTY_VALUE_FOR_IMPL_1 string
#define PROPERTY_VALUE_FOR_IMPL_2 opt_string

#define PROPERTY_VALUE_FOR_IMPL_17 type_info
#define PROPERTY_VALUE_FOR_IMPL_18 port_info

/* START command actual types */

// none
#define FTP_COMMAND_TYPE_COMMAND_CDUP FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_QUIT FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_REIN FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_PASV FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_STOU FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_ABOR FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_PWD FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_SYST FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_NOOP FTP_COMMAND_TYPE_NONE
#define FTP_COMMAND_TYPE_COMMAND_FEAT FTP_COMMAND_TYPE_NONE

//  string
#define FTP_COMMAND_TYPE_COMMAND_USER FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_PASS FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_ACCT FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_CWD FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_SMNT FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_RETR FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_STOR FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_APPE FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_RNFR FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_RNTO FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_DELE FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_RMD FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_MKD FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_SITE FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_AUTH FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_ADAT FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_MIC FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_CONF FTP_COMMAND_TYPE_STRING
#define FTP_COMMAND_TYPE_COMMAND_ENC FTP_COMMAND_TYPE_STRING

// optional string
#define FTP_COMMAND_TYPE_COMMAND_LIST FTP_COMMAND_TYPE_OPT_STRING
#define FTP_COMMAND_TYPE_COMMAND_NLST FTP_COMMAND_TYPE_OPT_STRING
#define FTP_COMMAND_TYPE_COMMAND_STAT FTP_COMMAND_TYPE_OPT_STRING
#define FTP_COMMAND_TYPE_COMMAND_HELP FTP_COMMAND_TYPE_OPT_STRING

// special types
#define FTP_COMMAND_TYPE_COMMAND_TYPE FTP_COMMAND_TYPE_TYPE_INFO
#define FTP_COMMAND_TYPE_COMMAND_PORT FTP_COMMAND_TYPE_PORT_INFO

/* END command types */

typedef struct {
	FtpCommandEnum type;
	union {
		tstr PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_STRING);
		OptionalString PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_OPT_STRING);
		FTPCommandTypeInformation* PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_TYPE_INFO);
		FTPPortInformation* PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_PORT_INFO);
	} data;

} FTPCommand;

NODISCARD FTPCommand* parse_single_ftp_command(BufferedReader* buffered_reader);

void free_ftp_command(FTPCommand* cmd);

NODISCARD const char* get_command_name(const FTPCommand* command);

NODISCARD FTPCommandTypeInformation* parse_ftp_command_type_info(tstr_view arg);
