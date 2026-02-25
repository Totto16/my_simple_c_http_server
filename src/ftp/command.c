#include "./command.h"
#include "generic/ip.h"
#include "generic/serialize.h"
#include "utils/utils.h"

#include <string.h>

#define MAKE_STRING_ARG(str) \
	do { \
		command->data.string = tstr_from_view(str); \
	} while(false)

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

	int result;

	bool correct = tstr_view_to_int(input, &result);

	if(!correct) {
		return false;
	}

	if(result < 0 ||
	   result > 0xFF) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
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

	*command = (FTPCommand){ .data = { 0 }, .type = 0 };

	// see https://datatracker.ietf.org/doc/html/rfc959 5.3.1
	if(tstr_view_eq_ignore_case(data_str, "CDUP")) {
		command->type = FtpCommandCdup;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "QUIT")) {
		command->type = FtpCommandQuit;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "REIN")) {
		command->type = FtpCommandRein;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "PASV")) {
		command->type = FtpCommandPasv;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "STOU")) {
		command->type = FtpCommandStou;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "ABOR")) {
		command->type = FtpCommandAbor;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "PWD")) {
		command->type = FtpCommandPwd;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "LIST")) {
		command->type = FtpCommandList;
		command->data.string = tstr_init(); // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "NLST")) {
		command->type = FtpCommandNlst;
		command->data.string = tstr_init(); // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "SYST")) {
		command->type = FtpCommandSyst;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "STAT")) {
		command->type = FtpCommandStat;
		command->data.string = tstr_init(); // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "HELP")) {
		command->type = FtpCommandHelp;
		command->data.string = tstr_init(); // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "NOOP")) {
		command->type = FtpCommandNoop;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "FEAT")) {
		command->type = FtpCommandFeat;
		return command;
	}

	const tstr_split_result split_result = tstr_split(data_str, " ");

	if(!split_result.ok) {
		free(command);
		return NULL;
	}

	const tstr_view command_str = split_result.first;
	const tstr_view argument_str = split_result.second;

	if(tstr_view_eq_ignore_case(command_str, "USER")) {
		command->type = FtpCommandUser;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "PASS")) {
		command->type = FtpCommandPass;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "ACCT")) {
		command->type = FtpCommandAcct;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "CWD")) {
		command->type = FtpCommandCwd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "SMNT")) {
		command->type = FtpCommandSmnt;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "RETR")) {
		command->type = FtpCommandRetr;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "STOR")) {
		command->type = FtpCommandStor;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "APPE")) {
		command->type = FtpCommandAppe;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "RNFR")) {
		command->type = FtpCommandRnfr;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "RNTO")) {
		command->type = FtpCommandRnto;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "DELE")) {
		command->type = FtpCommandDele;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "RMD")) {
		command->type = FtpCommandRmd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "MKD")) {
		command->type = FtpCommandMkd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "LIST")) {
		command->type = FtpCommandList;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "NLST")) {
		command->type = FtpCommandNlst;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "SITE")) {
		command->type = FtpCommandSite;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "STAT")) {
		command->type = FtpCommandStat;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "HELP")) {
		command->type = FtpCommandHelp;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "AUTH")) {
		command->type = FtpCommandAuth;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "ADAT")) {
		command->type = FtpCommandAdat;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "MIC")) {
		command->type = FtpCommandMic;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "CONF")) {
		command->type = FtpCommandConf;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "ENC")) {
		command->type = FtpCommandEnc;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "TYPE")) {
		command->type = FtpCommandType;
		FTPCommandTypeInformation* type_info = parse_ftp_command_type_info(argument_str);
		if(type_info == NULL) {
			free(command);
			return NULL;
		}
		command->data.type_info = type_info;
		return command;
	}

	if(tstr_view_eq_ignore_case(command_str, "PORT")) {
		command->type = FtpCommandPort;
		FTPPortInformation* port_info = parse_ftp_command_port_info(argument_str);
		if(port_info == NULL) {
			free(command);
			return NULL;
		}
		command->data.port_info = port_info;
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
			free(cmd->data.type_info);
			break;
		}
		case FtpCommandPort: {
			free(cmd->data.port_info);
			break;
		}
		// string arguments
		case FtpCommandUser:
		case FtpCommandPass:
		case FtpCommandAcct:
		case FtpCommandCwd:
		case FtpCommandSmnt:
		case FtpCommandRetr:
		case FtpCommandStor:
		case FtpCommandAppe:
		case FtpCommandRnfr:
		case FtpCommandRnto:
		case FtpCommandDele:
		case FtpCommandRmd:
		case FtpCommandMkd:
		case FtpCommandSite:
		case FtpCommandAuth:
		case FtpCommandAdat:
		case FtpCommandMic:
		case FtpCommandConf:
		case FtpCommandEnc: {
			tstr_free(&(cmd->data.string));
			break;
		}
		// optional string arguments
		case FtpCommandList:
		case FtpCommandNlst:
		case FtpCommandStat:
		case FtpCommandHelp: {
			if(tstr_data(&(cmd->data.string)) != NULL) {
				tstr_free(&(cmd->data.string));
			}
			break;
		}
		// no arguments
		case FtpCommandCdup:
		case FtpCommandRein:
		case FtpCommandPasv:
		case FtpCommandStou:
		case FtpCommandAbor:
		case FtpCommandPwd:
		case FtpCommandSyst:
		case FtpCommandNoop:
		case FtpCommandFeat:
		case FtpCommandQuit:
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
