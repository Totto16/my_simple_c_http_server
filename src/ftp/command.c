

#include "./command.h"
#include "generic/ip.h"
#include "generic/serialize.h"
#include "utils/utils.h"

#include <string.h>

#define MAKE_STRING_ARG(str) \
	do { \
		int str_length = strlen(str) + 1; \
		char* malloced_str = (char*)malloc(str_length); \
		if(malloced_str == NULL) { \
			free(command); \
			return NULL; \
		} \
		memcpy(malloced_str, str, str_length); \
		command->data.string = malloced_str; \
	} while(false)

FTPCommandTypeInformation* parse_ftp_command_type_info(const tstr* const arg) {
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

	size_t length = tstr_len(arg);

	const char* const data = tstr_cstr(arg);

	if(length < 1) {
		free(info);
		return NULL;
	}

	if(length == 1) {
		switch(data[0]) {
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

NODISCARD static bool parse_u8_into(char* input, uint8_t* result_addr) {

	if(strlen(input) > 3) {
		return false;
	}

	char* endpointer = NULL;

	errno = 0;
	long result =
	    strtol(input, &endpointer,
	           10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	// it isn't a number, if either errno is set or if the endpointer is not a '\0
	if(*endpointer != '\0') {
		return false;
	}

	if(errno != 0 || result < 0 ||
	   result > 0xFF) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		return false;
	}

	*result_addr = (uint8_t)result;
	return true;
}

static FTPPortInformation* parse_ftp_command_port_info(char* arg) {
	FTPPortInformation* info = (FTPPortInformation*)malloc(sizeof(FTPPortInformation));

	if(!info) {
		return NULL;
	}

	// return <host-port>
	// <host-port> ::= <host-number>,<port-number>
	// <host-number> ::= <number>,<number>,<number>,<number>
	// <port-number> ::= <number>,<number>
	// <number> ::= any decimal integer 1 through 255

	char* currently_at = arg;

	uint8_t result[MAX_PORT_ARG];

	for(int i = 0; i < MAX_PORT_ARG; ++i) {
		char* resulting_index = strstr(currently_at, ",");

		if(i == MAX_PORT_ARG - 1) {
			if(resulting_index != NULL) {
				free(info);
				return NULL;
			}
		} else {
			if(resulting_index == NULL) {
				free(info);
				return NULL;
			}

			*resulting_index = '\0';
		}

		bool success = parse_u8_into(currently_at, result + i);

		if(!success) {
			free(info);
			return NULL;
		}

		currently_at = resulting_index + 1;
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
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "NLST")) {
		command->type = FtpCommandNlst;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "SYST")) {
		command->type = FtpCommandSyst;
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "STAT")) {
		command->type = FtpCommandStat;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(tstr_view_eq_ignore_case(data_str, "HELP")) {
		command->type = FtpCommandHelp;
		command->data.string = NULL; // signifies, that this has an optional argument
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

	char* resulting_index = strstr(command_str, " ");

	if(resulting_index == NULL) {
		free(command);
		return NULL;
	}

	const tstr_view command_str = ;

	const tstr_view argument_str = resulting_index + 1;

	if(tstr_view_eq_ignore_case("USER", command_str)) {
		command->type = FtpCommandUser;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("PASS", command_str)) {
		command->type = FtpCommandPass;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("ACCT", command_str)) {
		command->type = FtpCommandAcct;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("CWD", command_str)) {
		command->type = FtpCommandCwd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("SMNT", command_str)) {
		command->type = FtpCommandSmnt;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("RETR", command_str)) {
		command->type = FtpCommandRetr;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("STOR", command_str)) {
		command->type = FtpCommandStor;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("APPE", command_str)) {
		command->type = FtpCommandAppe;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("RNFR", command_str)) {
		command->type = FtpCommandRnfr;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("RNTO", command_str)) {
		command->type = FtpCommandRnto;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("DELE", command_str)) {
		command->type = FtpCommandDele;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("RMD", command_str)) {
		command->type = FtpCommandRmd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("MKD", command_str)) {
		command->type = FtpCommandMkd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("LIST", command_str)) {
		command->type = FtpCommandList;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("NLST", command_str)) {
		command->type = FtpCommandNlst;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("SITE", command_str)) {
		command->type = FtpCommandSite;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("STAT", command_str)) {
		command->type = FtpCommandStat;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("HELP", command_str)) {
		command->type = FtpCommandHelp;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("AUTH", command_str)) {
		command->type = FtpCommandAuth;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("ADAT", command_str)) {
		command->type = FtpCommandAdat;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("MIC", command_str)) {
		command->type = FtpCommandMic;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("CONF", command_str)) {
		command->type = FtpCommandConf;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("ENC", command_str)) {
		command->type = FtpCommandEnc;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(tstr_view_eq_ignore_case("TYPE", command_str)) {
		command->type = FtpCommandType;
		FTPCommandTypeInformation* type_info = parse_ftp_command_type_info(argument_str);
		if(type_info == NULL) {
			free(command);
			return NULL;
		}
		command->data.type_info = type_info;
		return command;
	}

	if(tstr_view_eq_ignore_case("PORT", command_str)) {
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
			free(cmd->data.string);
			break;
		}
		// optional string arguments
		case FtpCommandList:
		case FtpCommandNlst:
		case FtpCommandStat:
		case FtpCommandHelp: {
			if(cmd->data.string) {
				free(cmd->data.string);
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

void free_ftp_command_array(FTPCommandArray* array) {
	if(array == NULL) {
		return;
	}

	for(size_t i = 0; i < TVEC_LENGTH(FTPCommandPtr, *array); ++i) {
		free_ftp_command(TVEC_AT(FTPCommandPtr, *array, i));
	}

	TVEC_FREE(FTPCommandPtr, array);
	free(array);
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
