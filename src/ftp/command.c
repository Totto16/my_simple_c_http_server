#include "./command.h"
#include "generic/ip.h"
#include "generic/serialize.h"
#include "utils/utils.h"

#include <string.h>

FTPCommandTypeInformation* parse_ftp_command_type_info(const tstr_view arg) {
	FTPCommandTypeInformation* info =
	    (FTPCommandTypeInformation*)malloc(sizeof(FTPCommandTypeInformation));

	if(!info) {
		return NULL;
	}

	info->is_normal = true;
	info->data.type = FtpTransmissionTypeNone;

	// return <type-code>
	// <type-code> ::= A [<sp> <form-code>]
	// 			  | E [<sp> <form-code>]
	// 			  | I
	// 			  | L <sp> <byte-size>
	// <form-code> ::= N | T | C
	// <byte-size> ::= <number>
	// <number> ::= any decimal integer 1 through 255

	if(arg.len < 1) {
		free(info);
		return NULL;
	}

	if(arg.len == 1) {
		switch(arg.data[0]) {
			case 'A': {
				info->is_normal = true;
				info->data.type = FtpTransmissionTypeAscii;
				return info;
			}
			case 'E': {
				info->is_normal = true;
				info->data.type = FtpTransmissionTypeEbcdic;
				return info;
			}
			case 'I': {
				info->is_normal = true;
				info->data.type = FtpTransmissionTypeImage;
				return info;
			}
			default: {
				free(info);
				return NULL;
			}
		}
	}

	// TODO(Totto): also parse other flags and longer strings
	free(info);
	return NULL;
}

#define MAX_PORT_ARG 6

NODISCARD static bool parse_u8_into(const tstr_view input, uint8_t* const result_addr) {

	if(input.len > 3) {
		return false;
	}

	int result = 0;

	bool correct = tstr_view_to_int(input, &result);

	if(!correct) {
		return false;
	}

	if(result < 0 || result > 0xFF) { // NOLINT(readability-magic-numbers)
		return false;
	}

	*result_addr = (uint8_t)result;
	return true;
}

static FTPPortInformation* parse_ftp_command_port_info(const tstr_view arg) {
	FTPPortInformation* info = (FTPPortInformation*)malloc(sizeof(FTPPortInformation));

	if(!info) {
		return NULL;
	}

	// return <host-port>
	// <host-port> ::= <host-number>,<port-number>
	// <host-number> ::= <number>,<number>,<number>,<number>
	// <port-number> ::= <number>,<number>
	// <number> ::= any decimal integer 1 through 255

	tstr_split_iter iter = tstr_split_init(arg, ",");

	uint8_t result[MAX_PORT_ARG];

	for(int i = 0; i < MAX_PORT_ARG; ++i) {
		tstr_view current_value;
		bool split_succeeded = tstr_split_next(&iter, &current_value);

		if(!split_succeeded) {
			free(info);
			return NULL;
		}

		bool success = parse_u8_into(current_value, result + i);

		if(!success) {
			free(info);
			return NULL;
		}
	}

	// after MAX_PORT_ARG splits we should be at the end
	if(!iter.finished) {
		free(info);
		return NULL;
	}

	IPV4Address addr = get_ipv4_address_from_host_bytes(result);

	const uint16_t port = deserialize_u16_le_to_host(result + 4);

	info->addr = addr;
	info->port = port;

	return info;
}

#define FTP_COMMAND_SEPERATORS "\r\n"

#define OPT_STRING_EMPTY ((OptionalString){ .has_value = false, .value = tstr_null() })

NODISCARD FTPCommand* parse_single_ftp_command(BufferedReader* const buffered_reader) {

	BufferedReadResult read_result =
	    buffered_reader_get_until_delimiter(buffered_reader, FTP_COMMAND_SEPERATORS);

	if(read_result.type != BufferedReadResultTypeOk) {
		return NULL;
	}

	const SizedBuffer data = read_result.value.buffer;

	if(data.size < 3) {
		return NULL;
	}

	// no need to free this, as it is owned by the buffered reader and freed at the end of that!
	tstr temp_non_owned_str = tstr_own(data.data, data.size, data.size);
	const tstr_view data_str = tstr_as_view(&temp_non_owned_str);

	FTPCommand* command = (FTPCommand*)malloc(sizeof(FTPCommand));
	if(!command) {
		return NULL;
	}

	*command = ZERO_STRUCT(FTPCommand);

	// see https://datatracker.ietf.org/doc/html/rfc959 5.3.1
	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("CDUP"))) {
		command->type = FtpCommandCdup;
		static_assert(FTP_COMMAND_TYPE_COMMAND_CDUP == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("QUIT"))) {
		command->type = FtpCommandQuit;
		static_assert(FTP_COMMAND_TYPE_COMMAND_QUIT == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("REIN"))) {
		command->type = FtpCommandRein;
		static_assert(FTP_COMMAND_TYPE_COMMAND_REIN == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("PASV"))) {
		command->type = FtpCommandPasv;
		static_assert(FTP_COMMAND_TYPE_COMMAND_PASV == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("STOU"))) {
		command->type = FtpCommandStou;
		static_assert(FTP_COMMAND_TYPE_COMMAND_STOU == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("ABOR"))) {
		command->type = FtpCommandAbor;
		static_assert(FTP_COMMAND_TYPE_COMMAND_ABOR == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("PWD"))) {
		command->type = FtpCommandPwd;
		static_assert(FTP_COMMAND_TYPE_COMMAND_PWD == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("LIST"))) {
		command->type = FtpCommandList;
		static_assert(FTP_COMMAND_TYPE_COMMAND_LIST == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_LIST) = OPT_STRING_EMPTY;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("NLST"))) {
		command->type = FtpCommandNlst;
		static_assert(FTP_COMMAND_TYPE_COMMAND_NLST == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_NLST) = OPT_STRING_EMPTY;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("SYST"))) {
		command->type = FtpCommandSyst;
		static_assert(FTP_COMMAND_TYPE_COMMAND_SYST == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("STAT"))) {
		command->type = FtpCommandStat;
		static_assert(FTP_COMMAND_TYPE_COMMAND_STAT == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_STAT) = OPT_STRING_EMPTY;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("HELP"))) {
		command->type = FtpCommandHelp;
		static_assert(FTP_COMMAND_TYPE_COMMAND_HELP == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_HELP) = OPT_STRING_EMPTY;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("NOOP"))) {
		command->type = FtpCommandNoop;
		static_assert(FTP_COMMAND_TYPE_COMMAND_NOOP == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, TSTR_TSV("FEAT"))) {
		command->type = FtpCommandFeat;
		static_assert(FTP_COMMAND_TYPE_COMMAND_FEAT == FTP_COMMAND_TYPE_NONE);
		return command;
	}

	const tstr_split_result split_result = tstr_split(data_str, " ");

	if(!split_result.ok) {
		free(command);
		return NULL;
	}

	const tstr_view command_str = split_result.first;
	const tstr_view argument_str = split_result.second;

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("USER"))) {
		command->type = FtpCommandUser;
		static_assert(FTP_COMMAND_TYPE_COMMAND_USER == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_USER) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("PASS"))) {
		command->type = FtpCommandPass;
		static_assert(FTP_COMMAND_TYPE_COMMAND_PASS == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_PASS) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("ACCT"))) {
		command->type = FtpCommandAcct;
		static_assert(FTP_COMMAND_TYPE_COMMAND_ACCT == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_ACCT) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("CWD"))) {
		command->type = FtpCommandCwd;
		static_assert(FTP_COMMAND_TYPE_COMMAND_CWD == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_CWD) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("SMNT"))) {
		command->type = FtpCommandSmnt;
		static_assert(FTP_COMMAND_TYPE_COMMAND_SMNT == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_SMNT) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("RETR"))) {
		command->type = FtpCommandRetr;
		static_assert(FTP_COMMAND_TYPE_COMMAND_RETR == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_RETR) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("STOR"))) {
		command->type = FtpCommandStor;
		static_assert(FTP_COMMAND_TYPE_COMMAND_STOR == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_STOR) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("APPE"))) {
		command->type = FtpCommandAppe;
		static_assert(FTP_COMMAND_TYPE_COMMAND_APPE == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_APPE) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("RNFR"))) {
		command->type = FtpCommandRnfr;
		static_assert(FTP_COMMAND_TYPE_COMMAND_RNFR == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_RNFR) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("RNTO"))) {
		command->type = FtpCommandRnto;
		static_assert(FTP_COMMAND_TYPE_COMMAND_RNTO == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_RNTO) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("DELE"))) {
		command->type = FtpCommandDele;
		static_assert(FTP_COMMAND_TYPE_COMMAND_DELE == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_DELE) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("RMD"))) {
		command->type = FtpCommandRmd;
		static_assert(FTP_COMMAND_TYPE_COMMAND_RMD == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_RMD) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("MKD"))) {
		command->type = FtpCommandMkd;
		static_assert(FTP_COMMAND_TYPE_COMMAND_MKD == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_MKD) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("LIST"))) {
		command->type = FtpCommandList;
		static_assert(FTP_COMMAND_TYPE_COMMAND_LIST == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_LIST) =
		    (OptionalString){ .has_value = true, .value = tstr_from_view(argument_str) };
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("NLST"))) {
		command->type = FtpCommandNlst;
		static_assert(FTP_COMMAND_TYPE_COMMAND_NLST == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_NLST) =
		    (OptionalString){ .has_value = true, .value = tstr_from_view(argument_str) };
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("SITE"))) {
		command->type = FtpCommandSite;
		static_assert(FTP_COMMAND_TYPE_COMMAND_SITE == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_SITE) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("STAT"))) {
		command->type = FtpCommandStat;
		static_assert(FTP_COMMAND_TYPE_COMMAND_STAT == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_STAT) =
		    (OptionalString){ .has_value = true, .value = tstr_from_view(argument_str) };
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("HELP"))) {
		command->type = FtpCommandHelp;
		static_assert(FTP_COMMAND_TYPE_COMMAND_HELP == FTP_COMMAND_TYPE_OPT_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_HELP) =
		    (OptionalString){ .has_value = true, .value = tstr_from_view(argument_str) };
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("AUTH"))) {
		command->type = FtpCommandAuth;
		static_assert(FTP_COMMAND_TYPE_COMMAND_AUTH == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_AUTH) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("ADAT"))) {
		command->type = FtpCommandAdat;
		static_assert(FTP_COMMAND_TYPE_COMMAND_ADAT == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_ADAT) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("MIC"))) {
		command->type = FtpCommandMic;
		static_assert(FTP_COMMAND_TYPE_COMMAND_MIC == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_MIC) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("CONF"))) {
		command->type = FtpCommandConf;
		static_assert(FTP_COMMAND_TYPE_COMMAND_CONF == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_CONF) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("ENC"))) {
		command->type = FtpCommandEnc;
		static_assert(FTP_COMMAND_TYPE_COMMAND_ENC == FTP_COMMAND_TYPE_STRING);
		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_ENC) =
		    tstr_from_view(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("TYPE"))) {
		command->type = FtpCommandType;
		static_assert(FTP_COMMAND_TYPE_COMMAND_TYPE == FTP_COMMAND_TYPE_TYPE_INFO);
		FTPCommandTypeInformation* type_info = parse_ftp_command_type_info(argument_str);
		if(type_info == NULL) {
			free(command);
			return NULL;
		}

		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_TYPE) = type_info;
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, TSTR_TSV("PORT"))) {
		command->type = FtpCommandPort;
		static_assert(FTP_COMMAND_TYPE_COMMAND_PORT == FTP_COMMAND_TYPE_PORT_INFO);
		FTPPortInformation* port_info = parse_ftp_command_port_info(argument_str);
		if(port_info == NULL) {
			free(command);
			return NULL;
		}

		command->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_COMMAND_PORT) = port_info;
		return command;
	}

	// TODO(Totto): implement these
	//     STRU <SP> <structure-code> <CRLF>
	//     MODE <SP> <mode-code> <CRLF>
	//     ALLO <SP> <decimal-integer> [<SP> R <SP> <decimal-integer>] <CRLF>
	//     REST <SP> <marker> <CRLF>

	//     PROT <SP> <prot-code> <CRLF>
	//     PBSZ <SP> <decimal-integer> <CRLF>

	free(command);
	return NULL;
}

void free_ftp_command(FTPCommand* cmd) {
	switch(cmd->type) {
		// special things
		case FtpCommandType: {
			static_assert(FTP_COMMAND_TYPE_COMMAND_TYPE == FTP_COMMAND_TYPE_TYPE_INFO);

			free(cmd->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_TYPE_INFO));
			break;
		}
		case FtpCommandPort: {
			static_assert(FTP_COMMAND_TYPE_COMMAND_PORT == FTP_COMMAND_TYPE_PORT_INFO);

			free(cmd->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_PORT_INFO));
			break;
		}
		// string arguments
		case FtpCommandUser:
			static_assert(FTP_COMMAND_TYPE_COMMAND_USER == FTP_COMMAND_TYPE_STRING);
		case FtpCommandPass:
			static_assert(FTP_COMMAND_TYPE_COMMAND_PASS == FTP_COMMAND_TYPE_STRING);
		case FtpCommandAcct:
			static_assert(FTP_COMMAND_TYPE_COMMAND_ACCT == FTP_COMMAND_TYPE_STRING);
		case FtpCommandCwd: static_assert(FTP_COMMAND_TYPE_COMMAND_CWD == FTP_COMMAND_TYPE_STRING);
		case FtpCommandSmnt:
			static_assert(FTP_COMMAND_TYPE_COMMAND_SMNT == FTP_COMMAND_TYPE_STRING);
		case FtpCommandRetr:
			static_assert(FTP_COMMAND_TYPE_COMMAND_RETR == FTP_COMMAND_TYPE_STRING);
		case FtpCommandStor:
			static_assert(FTP_COMMAND_TYPE_COMMAND_STOR == FTP_COMMAND_TYPE_STRING);
		case FtpCommandAppe:
			static_assert(FTP_COMMAND_TYPE_COMMAND_APPE == FTP_COMMAND_TYPE_STRING);
		case FtpCommandRnfr:
			static_assert(FTP_COMMAND_TYPE_COMMAND_RNFR == FTP_COMMAND_TYPE_STRING);
		case FtpCommandRnto:
			static_assert(FTP_COMMAND_TYPE_COMMAND_RNTO == FTP_COMMAND_TYPE_STRING);
		case FtpCommandDele:
			static_assert(FTP_COMMAND_TYPE_COMMAND_DELE == FTP_COMMAND_TYPE_STRING);
		case FtpCommandRmd: static_assert(FTP_COMMAND_TYPE_COMMAND_RMD == FTP_COMMAND_TYPE_STRING);
		case FtpCommandMkd: static_assert(FTP_COMMAND_TYPE_COMMAND_MKD == FTP_COMMAND_TYPE_STRING);
		case FtpCommandSite:
			static_assert(FTP_COMMAND_TYPE_COMMAND_SITE == FTP_COMMAND_TYPE_STRING);
		case FtpCommandAuth:
			static_assert(FTP_COMMAND_TYPE_COMMAND_AUTH == FTP_COMMAND_TYPE_STRING);
		case FtpCommandAdat:
			static_assert(FTP_COMMAND_TYPE_COMMAND_ADAT == FTP_COMMAND_TYPE_STRING);
		case FtpCommandMic: static_assert(FTP_COMMAND_TYPE_COMMAND_MIC == FTP_COMMAND_TYPE_STRING);
		case FtpCommandConf:
			static_assert(FTP_COMMAND_TYPE_COMMAND_CONF == FTP_COMMAND_TYPE_STRING);
		case FtpCommandEnc: {
			static_assert(FTP_COMMAND_TYPE_COMMAND_ENC == FTP_COMMAND_TYPE_STRING);

			tstr_free(&(cmd->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_STRING)));
			break;
		}
		// optional string arguments
		case FtpCommandList:
			static_assert(FTP_COMMAND_TYPE_COMMAND_LIST == FTP_COMMAND_TYPE_OPT_STRING);
		case FtpCommandNlst:
			static_assert(FTP_COMMAND_TYPE_COMMAND_NLST == FTP_COMMAND_TYPE_OPT_STRING);
		case FtpCommandStat:
			static_assert(FTP_COMMAND_TYPE_COMMAND_STAT == FTP_COMMAND_TYPE_OPT_STRING);
		case FtpCommandHelp: {
			static_assert(FTP_COMMAND_TYPE_COMMAND_HELP == FTP_COMMAND_TYPE_OPT_STRING);

			if(cmd->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_OPT_STRING).has_value) {
				tstr_free(&(cmd->data.PROPERTY_VALUE_FOR(FTP_COMMAND_TYPE_OPT_STRING).value));
			}
			break;
		}
		// no arguments
		case FtpCommandCdup: static_assert(FTP_COMMAND_TYPE_COMMAND_CDUP == FTP_COMMAND_TYPE_NONE);
		case FtpCommandRein: static_assert(FTP_COMMAND_TYPE_COMMAND_REIN == FTP_COMMAND_TYPE_NONE);
		case FtpCommandPasv: static_assert(FTP_COMMAND_TYPE_COMMAND_PASV == FTP_COMMAND_TYPE_NONE);
		case FtpCommandStou: static_assert(FTP_COMMAND_TYPE_COMMAND_STOU == FTP_COMMAND_TYPE_NONE);
		case FtpCommandAbor: static_assert(FTP_COMMAND_TYPE_COMMAND_ABOR == FTP_COMMAND_TYPE_NONE);
		case FtpCommandPwd: static_assert(FTP_COMMAND_TYPE_COMMAND_PWD == FTP_COMMAND_TYPE_NONE);
		case FtpCommandSyst: static_assert(FTP_COMMAND_TYPE_COMMAND_SYST == FTP_COMMAND_TYPE_NONE);
		case FtpCommandNoop: static_assert(FTP_COMMAND_TYPE_COMMAND_NOOP == FTP_COMMAND_TYPE_NONE);
		case FtpCommandFeat: static_assert(FTP_COMMAND_TYPE_COMMAND_FEAT == FTP_COMMAND_TYPE_NONE);
		case FtpCommandQuit: static_assert(FTP_COMMAND_TYPE_COMMAND_QUIT == FTP_COMMAND_TYPE_NONE);
		default: break;
	}
}

const char* get_command_name(const FTPCommand* const command) {
	switch(command->type) {
		case FtpCommandUser: return "USER";
		case FtpCommandPass: return "PASS";
		case FtpCommandAcct: return "ACCT";
		case FtpCommandCwd: return "CWD";
		case FtpCommandCdup: return "CDUP";
		case FtpCommandSmnt: return "SMNT";
		case FtpCommandQuit: return "QUIT";
		case FtpCommandRein: return "REIN";
		case FtpCommandPort: return "PORT";
		case FtpCommandPasv: return "PASV";
		case FtpCommandType: return "TYPE";
		case FtpCommandStru: return "STRU";
		case FtpCommandMode: return "MODE";
		case FtpCommandRetr: return "RETR";
		case FtpCommandStor: return "STOR";
		case FtpCommandStou: return "STOU";
		case FtpCommandAppe: return "APPE";
		case FtpCommandAllo: return "ALLO";
		case FtpCommandRest: return "REST";
		case FtpCommandRnfr: return "RNFR";
		case FtpCommandRnto: return "RNTO";
		case FtpCommandAbor: return "ABOR";
		case FtpCommandDele: return "DELE";
		case FtpCommandRmd: return "RMD";
		case FtpCommandMkd: return "MKD";
		case FtpCommandPwd: return "PWD";
		case FtpCommandList: return "LIST";
		case FtpCommandNlst: return "NLST";
		case FtpCommandSite: return "SITE";
		case FtpCommandSyst: return "SYST";
		case FtpCommandStat: return "STAT";
		case FtpCommandHelp: return "HELP";
		case FtpCommandNoop: return "NOOP";
		case FtpCommandAuth: return "AUTH";
		case FtpCommandAdat: return "ADAT";
		case FtpCommandProt: return "PROT";
		case FtpCommandPbsz: return "PBSZ";
		case FtpCommandMic: return "MIC";
		case FtpCommandConf: return "CONF";
		case FtpCommandEnc: return "ENC";
		case FtpCommandFeat: return "FEAT";
		case FtpCommandOpts: return "OPTS";
		default: return "<UNKNOWN COMMAND>";
	}
}
