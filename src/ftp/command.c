

#include "./command.h"
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

FTPCommandTypeInformation* parse_ftp_command_type_info(char* arg) {
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

	size_t length = strlen(arg);

	if(length < 1) {
		free(info);
		return NULL;
	}

	if(length == 1) {
		switch(arg[0]) {
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

	uint32_t addr = result[0];
	addr = (addr << 8) + // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	       result[1];
	addr = (addr << 8) + // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	       result[2];
	addr = (addr << 8) + // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	       result[3];

	uint16_t port = result[4];
	addr = (addr << 8) + // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	       result[5];    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	info->addr = addr;
	info->port = port;

	return info;
}

NODISCARD static FTPCommand* parse_single_ftp_command(char* command_str) {

	size_t length = strlen(command_str);

	if(length < 3) {
		return NULL;
	}

	FTPCommand* command = (FTPCommand*)malloc_with_memset(sizeof(FTPCommand), true);
	if(!command) {
		return NULL;
	}

	// see https://datatracker.ietf.org/doc/html/rfc959 5.3.1
	if(strcasecmp("CDUP", command_str) == 0) {
		command->type = FtpCommandCdup;
		return command;
	}

	if(strcasecmp("QUIT", command_str) == 0) {
		command->type = FtpCommandQuit;
		return command;
	}

	if(strcasecmp("REIN", command_str) == 0) {
		command->type = FtpCommandRein;
		return command;
	}

	if(strcasecmp("PASV", command_str) == 0) {
		command->type = FtpCommandPasv;
		return command;
	}

	if(strcasecmp("STOU", command_str) == 0) {
		command->type = FtpCommandStou;
		return command;
	}

	if(strcasecmp("ABOR", command_str) == 0) {
		command->type = FtpCommandAbor;
		return command;
	}

	if(strcasecmp("PWD", command_str) == 0) {
		command->type = FtpCommandPwd;
		return command;
	}

	if(strcasecmp("LIST", command_str) == 0) {
		command->type = FtpCommandList;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("NLST", command_str) == 0) {
		command->type = FtpCommandNlst;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("SYST", command_str) == 0) {
		command->type = FtpCommandSyst;
		return command;
	}

	if(strcasecmp("STAT", command_str) == 0) {
		command->type = FtpCommandStat;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("HELP", command_str) == 0) {
		command->type = FtpCommandHelp;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("NOOP", command_str) == 0) {
		command->type = FtpCommandNoop;
		return command;
	}

	if(strcasecmp("FEAT", command_str) == 0) {
		command->type = FtpCommandFeat;
		return command;
	}

	char* resulting_index = strstr(command_str, " ");

	if(resulting_index == NULL) {
		free(command);
		return NULL;
	}

	// overwrite the " " by a 0 terminator, so that it can be treated as normal string
	*resulting_index = '\0';

	char* argument_str = resulting_index + 1;

	if(strcasecmp("USER", command_str) == 0) {
		command->type = FtpCommandUser;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("PASS", command_str) == 0) {
		command->type = FtpCommandPass;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("ACCT", command_str) == 0) {
		command->type = FtpCommandAcct;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("CWD", command_str) == 0) {
		command->type = FtpCommandCwd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("SMNT", command_str) == 0) {
		command->type = FtpCommandSmnt;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("RETR", command_str) == 0) {
		command->type = FtpCommandRetr;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("STOR", command_str) == 0) {
		command->type = FtpCommandStor;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("APPE", command_str) == 0) {
		command->type = FtpCommandAppe;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("RNFR", command_str) == 0) {
		command->type = FtpCommandRnfr;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("RNTO", command_str) == 0) {
		command->type = FtpCommandRnto;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("DELE", command_str) == 0) {
		command->type = FtpCommandDele;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("RMD", command_str) == 0) {
		command->type = FtpCommandRmd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("MKD", command_str) == 0) {
		command->type = FtpCommandMkd;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("LIST", command_str) == 0) {
		command->type = FtpCommandList;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("NLST", command_str) == 0) {
		command->type = FtpCommandNlst;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("SITE", command_str) == 0) {
		command->type = FtpCommandSite;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("STAT", command_str) == 0) {
		command->type = FtpCommandStat;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("HELP", command_str) == 0) {
		command->type = FtpCommandHelp;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("AUTH", command_str) == 0) {
		command->type = FtpCommandAuth;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("ADAT", command_str) == 0) {
		command->type = FtpCommandAdat;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("MIC", command_str) == 0) {
		command->type = FtpCommandMic;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("CONF", command_str) == 0) {
		command->type = FtpCommandConf;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("ENC", command_str) == 0) {
		command->type = FtpCommandEnc;
		MAKE_STRING_ARG(argument_str);
		return command;
	}

	if(strcasecmp("TYPE", command_str) == 0) {
		command->type = FtpCommandType;
		FTPCommandTypeInformation* type_info = parse_ftp_command_type_info(argument_str);
		if(type_info == NULL) {
			free(command);
			return NULL;
		}
		command->data.type_info = type_info;
		return command;
	}

	if(strcasecmp("PORT", command_str) == 0) {
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

FTPCommandArray* parse_multiple_ftp_commands(char* raw_ftp_commands) {

#define FREE_AT_END() \
	do { \
		free(raw_ftp_commands); \
	} while(false)

	FTPCommandArray* array = malloc(sizeof(FTPCommandArray));

	if(!array) {
		return NULL;
	}

	*array = ZVEC_EMPTY(FTPCommandPtr);

	const char* const separators = "\r\n";
	size_t separators_length = strlen(separators);

	size_t size_to_proccess = strlen(raw_ftp_commands);
	char* currently_at = raw_ftp_commands;

	if(size_to_proccess == 0) {
		return array;
	}

	while(size_to_proccess > 0) {

		char* resulting_index = strstr(currently_at, separators);
		// no"\r\n" could be found, so a parse Error occurred, a NULL signals that
		if(resulting_index == NULL) {
			free_ftp_command_array(array);
			FREE_AT_END();
			return NULL;
		}

		size_t length = resulting_index - currently_at;

		// overwrite this, so that this is the end of ths string
		*resulting_index = '\0';

		FTPCommand* command = parse_single_ftp_command(currently_at);

		if(!command) {
			free_ftp_command_array(array);
			FREE_AT_END();
			return NULL;
		}

		auto _ = ZVEC_PUSH(FTPCommandPtr, array, command);
		UNUSED(_);

		size_t actual_length = length + separators_length;
		size_to_proccess -= actual_length;
		currently_at += actual_length;
	}

	if(size_to_proccess != 0) {
		free_ftp_command_array(array);
		FREE_AT_END();
		return NULL;
	}

	FREE_AT_END();
	return array;
}

#undef FREE_AT_END

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

	for(size_t i = 0; i < ZVEC_LENGTH(*array); ++i) {
		free_ftp_command(ZVEC_AT(FTPCommandPtr, *array, i));
	}

	ZVEC_FREE(FTPCommandPtr, array);
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
