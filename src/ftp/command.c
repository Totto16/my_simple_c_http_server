

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
	info->data.type = FTP_TRANSMISSION_TYPE_NONE;

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
				info->data.type = FTP_TRANSMISSION_TYPE_ASCII;
				return info;
			}
			case 'E': {
				info->is_normal = true;
				info->data.type = FTP_TRANSMISSION_TYPE_EBCDIC;
				return info;
			}
			case 'I': {
				info->is_normal = true;
				info->data.type = FTP_TRANSMISSION_TYPE_IMAGE;
				return info;
			}
			default: {
				free(info);
				return NULL;
			}
		}

		UNREACHABLE();
	}

	// TODO(Totto): also parse other flags and longer strings
	free(info);
	return NULL;
}

#define MAX_PORT_ARG 6

bool parseU8Into(char* input, uint8_t* result_addr) {

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

FTPPortInformation* parse_ftp_command_port_info(char* arg) {
	FTPPortInformation* info = (FTPPortInformation*)malloc(sizeof(FTPPortInformation));

	if(!info) {
		return NULL;
	}

	// return <host-port>
	// <host-port> ::= <host-number>,<port-number>
	// <host-number> ::= <number>,<number>,<number>,<number>
	// <port-number> ::= <number>,<number>
	// <number> ::= any decimal integer 1 through 255

	char* currentlyAt = arg;

	uint8_t result[MAX_PORT_ARG];

	for(int i = 0; i < MAX_PORT_ARG; ++i) {
		char* resultingIndex = strstr(currentlyAt, ",");

		if(i == MAX_PORT_ARG - 1) {
			if(resultingIndex != NULL) {
				free(info);
				return NULL;
			}
		} else {
			if(resultingIndex == NULL) {
				free(info);
				return NULL;
			}

			*resultingIndex = '\0';
		}

		bool success = parseU8Into(currentlyAt, result + i);

		if(!success) {
			free(info);
			return NULL;
		}

		currentlyAt = resultingIndex + 1;
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

FTPCommand* parseSingleFTPCommand(char* commandStr) {

	size_t length = strlen(commandStr);

	if(length < 3) {
		return NULL;
	}

	FTPCommand* command = (FTPCommand*)mallocWithMemset(sizeof(FTPCommand), true);
	if(!command) {
		return NULL;
	}

	// see https://datatracker.ietf.org/doc/html/rfc959 5.3.1
	if(strcasecmp("CDUP", commandStr) == 0) {
		command->type = FTP_COMMAND_CDUP;
		return command;
	}

	if(strcasecmp("QUIT", commandStr) == 0) {
		command->type = FTP_COMMAND_QUIT;
		return command;
	}

	if(strcasecmp("REIN", commandStr) == 0) {
		command->type = FTP_COMMAND_REIN;
		return command;
	}

	if(strcasecmp("PASV", commandStr) == 0) {
		command->type = FTP_COMMAND_PASV;
		return command;
	}

	if(strcasecmp("STOU", commandStr) == 0) {
		command->type = FTP_COMMAND_STOU;
		return command;
	}

	if(strcasecmp("ABOR", commandStr) == 0) {
		command->type = FTP_COMMAND_ABOR;
		return command;
	}

	if(strcasecmp("PWD", commandStr) == 0) {
		command->type = FTP_COMMAND_PWD;
		return command;
	}

	if(strcasecmp("LIST", commandStr) == 0) {
		command->type = FTP_COMMAND_LIST;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("NLST", commandStr) == 0) {
		command->type = FTP_COMMAND_NLST;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("SYST", commandStr) == 0) {
		command->type = FTP_COMMAND_SYST;
		return command;
	}

	if(strcasecmp("STAT", commandStr) == 0) {
		command->type = FTP_COMMAND_STAT;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("HELP", commandStr) == 0) {
		command->type = FTP_COMMAND_HELP;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	}

	if(strcasecmp("NOOP", commandStr) == 0) {
		command->type = FTP_COMMAND_NOOP;
		return command;
	}

	if(strcasecmp("FEAT", commandStr) == 0) {
		command->type = FTP_COMMAND_FEAT;
		return command;
	}

	char* resultingIndex = strstr(commandStr, " ");

	if(resultingIndex == NULL) {
		free(command);
		return NULL;
	}

	// overwrite the " " by a 0 terminator, so that it can be treated as normal string
	*resultingIndex = '\0';

	char* argumentStr = resultingIndex + 1;

	if(strcasecmp("USER", commandStr) == 0) {
		command->type = FTP_COMMAND_USER;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("PASS", commandStr) == 0) {
		command->type = FTP_COMMAND_PASS;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("ACCT", commandStr) == 0) {
		command->type = FTP_COMMAND_ACCT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("CWD", commandStr) == 0) {
		command->type = FTP_COMMAND_CWD;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("SMNT", commandStr) == 0) {
		command->type = FTP_COMMAND_SMNT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("RETR", commandStr) == 0) {
		command->type = FTP_COMMAND_RETR;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("STOR", commandStr) == 0) {
		command->type = FTP_COMMAND_STOR;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("APPE", commandStr) == 0) {
		command->type = FTP_COMMAND_APPE;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("RNFR", commandStr) == 0) {
		command->type = FTP_COMMAND_RNFR;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("RNTO", commandStr) == 0) {
		command->type = FTP_COMMAND_RNTO;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("DELE", commandStr) == 0) {
		command->type = FTP_COMMAND_DELE;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("RMD", commandStr) == 0) {
		command->type = FTP_COMMAND_RMD;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("MKD", commandStr) == 0) {
		command->type = FTP_COMMAND_MKD;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("LIST", commandStr) == 0) {
		command->type = FTP_COMMAND_LIST;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("NLST", commandStr) == 0) {
		command->type = FTP_COMMAND_NLST;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("SITE", commandStr) == 0) {
		command->type = FTP_COMMAND_SITE;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("STAT", commandStr) == 0) {
		command->type = FTP_COMMAND_STAT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("HELP", commandStr) == 0) {
		command->type = FTP_COMMAND_HELP;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("AUTH", commandStr) == 0) {
		command->type = FTP_COMMAND_AUTH;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("ADAT", commandStr) == 0) {
		command->type = FTP_COMMAND_ADAT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("MIC", commandStr) == 0) {
		command->type = FTP_COMMAND_MIC;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("CONF", commandStr) == 0) {
		command->type = FTP_COMMAND_CONF;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("ENC", commandStr) == 0) {
		command->type = FTP_COMMAND_ENC;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	if(strcasecmp("TYPE", commandStr) == 0) {
		command->type = FTP_COMMAND_TYPE;
		FTPCommandTypeInformation* type_info = parse_ftp_command_type_info(argumentStr);
		if(type_info == NULL) {
			free(command);
			return NULL;
		}
		command->data.type_info = type_info;
		return command;
	}

	if(strcasecmp("PORT", commandStr) == 0) {
		command->type = FTP_COMMAND_PORT;
		FTPPortInformation* port_info = parse_ftp_command_port_info(argumentStr);
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

FTPCommandArray* parseMultipleFTPCommands(char* rawFtpCommands) {

#define FREE_AT_END() \
	do { \
		free(rawFtpCommands); \
	} while(false)

	FTPCommandArray* array = (FTPCommandArray*)malloc(sizeof(FTPCommandArray));
	if(!array) {
		FREE_AT_END();
		return NULL;
	}

	array->content = NULL;
	array->size = 0;

	const char* const separators = "\r\n";
	size_t separatorsLength = strlen(separators);

	size_t size_to_proccess = strlen(rawFtpCommands);
	char* currentlyAt = rawFtpCommands;

	if(size_to_proccess == 0) {
		return array;
	}

	while(size_to_proccess > 0) {

		char* resultingIndex = strstr(currentlyAt, separators);
		// no"\r\n" could be found, so a parse Error occurred, a NULL signals that
		if(resultingIndex == NULL) {
			freeFTPCommandArray(array);
			FREE_AT_END();
			return NULL;
		}

		size_t length = resultingIndex - currentlyAt;

		// overwrite this, so that this is the end of ths string
		*resultingIndex = '\0';

		FTPCommand* command = parseSingleFTPCommand(currentlyAt);

		if(!command) {
			freeFTPCommandArray(array);
			FREE_AT_END();
			return NULL;
		}

		array->size++;

		FTPCommand** new_content =
		    (FTPCommand**)realloc((void*)array->content, array->size * sizeof(FTPCommand*));
		if(!new_content) {
			freeFTPCommandArray(array);
			FREE_AT_END();
			return NULL;
		}

		array->content = new_content;

		array->content[array->size - 1] = command;

		size_t actualLength = length + separatorsLength;
		size_to_proccess -= actualLength;
		currentlyAt += actualLength;
	}

	if(size_to_proccess != 0) {
		freeFTPCommandArray(array);
		FREE_AT_END();
		return NULL;
	}

	FREE_AT_END();
	return array;
}

#undef FREE_AT_END

void freeFTPCommand(FTPCommand* cmd) {
	switch(cmd->type) {
		// special things
		case FTP_COMMAND_TYPE: {
			free(cmd->data.type_info);
			break;
		}
		case FTP_COMMAND_PORT: {
			free(cmd->data.port_info);
			break;
		}
		// string arguments
		case FTP_COMMAND_USER:
		case FTP_COMMAND_PASS:
		case FTP_COMMAND_ACCT:
		case FTP_COMMAND_CWD:
		case FTP_COMMAND_SMNT:
		case FTP_COMMAND_RETR:
		case FTP_COMMAND_STOR:
		case FTP_COMMAND_APPE:
		case FTP_COMMAND_RNFR:
		case FTP_COMMAND_RNTO:
		case FTP_COMMAND_DELE:
		case FTP_COMMAND_RMD:
		case FTP_COMMAND_MKD:
		case FTP_COMMAND_SITE:
		case FTP_COMMAND_AUTH:
		case FTP_COMMAND_ADAT:
		case FTP_COMMAND_MIC:
		case FTP_COMMAND_CONF:
		case FTP_COMMAND_ENC: {
			free(cmd->data.string);
			break;
		}
		// optional string arguments
		case FTP_COMMAND_LIST:
		case FTP_COMMAND_NLST:
		case FTP_COMMAND_STAT:
		case FTP_COMMAND_HELP: {
			if(cmd->data.string) {
				free(cmd->data.string);
			}
			break;
		}
		// no arguments
		case FTP_COMMAND_CDUP:
		case FTP_COMMAND_REIN:
		case FTP_COMMAND_PASV:
		case FTP_COMMAND_STOU:
		case FTP_COMMAND_ABOR:
		case FTP_COMMAND_PWD:
		case FTP_COMMAND_SYST:
		case FTP_COMMAND_NOOP:
		case FTP_COMMAND_FEAT:
		case FTP_COMMAND_QUIT:
		default: break;
	}
}

void freeFTPCommandArray(FTPCommandArray* array) {
	if(array == NULL) {
		return;
	}

	if(array->content != NULL) {
		for(size_t i = 0; i < array->size; ++i) {
			freeFTPCommand(array->content[i]);
		}
	}

	free(array);
}

const char* get_command_name(const FTPCommand* const command) {
	switch(command->type) {
		case FTP_COMMAND_TYPE: return "TYPE";
		case FTP_COMMAND_PORT: return "PORT";
		case FTP_COMMAND_USER: return "USER";
		case FTP_COMMAND_PASS: return "PASS";
		case FTP_COMMAND_ACCT: return "ACCT";
		case FTP_COMMAND_CWD: return "CWD";
		case FTP_COMMAND_SMNT: return "SMNT";
		case FTP_COMMAND_RETR: return "RETR";
		case FTP_COMMAND_STOR: return "STOR";
		case FTP_COMMAND_APPE: return "APPE";
		case FTP_COMMAND_RNFR: return "RNFR";
		case FTP_COMMAND_RNTO: return "RNTO";
		case FTP_COMMAND_DELE: return "DELE";
		case FTP_COMMAND_RMD: return "RMD";
		case FTP_COMMAND_MKD: return "MKD";
		case FTP_COMMAND_SITE: return "SITE";
		case FTP_COMMAND_AUTH: return "AUTH";
		case FTP_COMMAND_ADAT: return "ADAT";
		case FTP_COMMAND_MIC: return "MIC";
		case FTP_COMMAND_CONF: return "CONF";
		case FTP_COMMAND_ENC: return "ENC";
		case FTP_COMMAND_LIST: return "LIST";
		case FTP_COMMAND_NLST: return "NLST";
		case FTP_COMMAND_STAT: return "STAT";
		case FTP_COMMAND_HELP: return "HELP";
		case FTP_COMMAND_CDUP: return "CDUP";
		case FTP_COMMAND_REIN: return "REIN";
		case FTP_COMMAND_PASV: return "PASV";
		case FTP_COMMAND_STOU: return "STOU";
		case FTP_COMMAND_ABOR: return "ABOR";
		case FTP_COMMAND_PWD: return "PWD";
		case FTP_COMMAND_SYST: return "SYST";
		case FTP_COMMAND_NOOP: return "NOOP";
		case FTP_COMMAND_FEAT: return "FEAT";
		case FTP_COMMAND_QUIT: return "QUIT";
		default: return "<UNKNOWN COMMAND>";
	}
}
