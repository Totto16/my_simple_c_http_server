

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

FTPCommand* parseSingleFTPCommand(char* commandStr) {

	int length = strlen(commandStr);

	if(length < 3) {
		return NULL;
	}

	FTPCommand* command = (FTPCommand*)mallocWithMemset(sizeof(FTPCommand), true);
	if(!command) {
		return NULL;
	}

	// see https://datatracker.ietf.org/doc/html/rfc959 5.3.1
	if(strcmp("CDUP", commandStr) == 0) {
		command->type = FTP_COMMAND_CDUP;
		return command;
	} else if(strcmp("QUIT", commandStr) == 0) {
		command->type = FTP_COMMAND_QUIT;
		return command;
	} else if(strcmp("REIN", commandStr) == 0) {
		command->type = FTP_COMMAND_REIN;
		return command;
	} else if(strcmp("PASV", commandStr) == 0) {
		command->type = FTP_COMMAND_PASV;
		return command;
	} else if(strcmp("STOU", commandStr) == 0) {
		command->type = FTP_COMMAND_STOU;
		return command;
	} else if(strcmp("ABOR", commandStr) == 0) {
		command->type = FTP_COMMAND_ABOR;
		return command;
	} else if(strcmp("PWD", commandStr) == 0) {
		command->type = FTP_COMMAND_PWD;
		return command;
	} else if(strcmp("LIST", commandStr) == 0) {
		command->type = FTP_COMMAND_LIST;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	} else if(strcmp("NLST", commandStr) == 0) {
		command->type = FTP_COMMAND_NLST;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	} else if(strcmp("SYST", commandStr) == 0) {
		command->type = FTP_COMMAND_SYST;
		return command;
	} else if(strcmp("STAT", commandStr) == 0) {
		command->type = FTP_COMMAND_STAT;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	} else if(strcmp("HELP", commandStr) == 0) {
		command->type = FTP_COMMAND_HELP;
		command->data.string = NULL; // signifies, that this has an optional argument
		return command;
	} else if(strcmp("NOOP", commandStr) == 0) {
		command->type = FTP_COMMAND_NOOP;
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

	if(strcmp("USER", commandStr) == 0) {
		command->type = FTP_COMMAND_USER;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("PASS", commandStr) == 0) {
		command->type = FTP_COMMAND_PASS;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("ACCT", commandStr) == 0) {
		command->type = FTP_COMMAND_ACCT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("CWD", commandStr) == 0) {
		command->type = FTP_COMMAND_CWD;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("SMNT", commandStr) == 0) {
		command->type = FTP_COMMAND_SMNT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("RETR", commandStr) == 0) {
		command->type = FTP_COMMAND_RETR;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("STOR", commandStr) == 0) {
		command->type = FTP_COMMAND_STOR;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("APPE", commandStr) == 0) {
		command->type = FTP_COMMAND_APPE;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("RNFR", commandStr) == 0) {
		command->type = FTP_COMMAND_RNFR;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("RNTO", commandStr) == 0) {
		command->type = FTP_COMMAND_RNTO;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("DELE", commandStr) == 0) {
		command->type = FTP_COMMAND_DELE;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("RMD", commandStr) == 0) {
		command->type = FTP_COMMAND_RMD;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("MKD", commandStr) == 0) {
		command->type = FTP_COMMAND_MKD;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("LIST", commandStr) == 0) {
		command->type = FTP_COMMAND_LIST;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("NLST", commandStr) == 0) {
		command->type = FTP_COMMAND_NLST;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("SITE", commandStr) == 0) {
		command->type = FTP_COMMAND_SITE;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("STAT", commandStr) == 0) {
		command->type = FTP_COMMAND_STAT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("HELP", commandStr) == 0) {
		command->type = FTP_COMMAND_HELP;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("AUTH", commandStr) == 0) {
		command->type = FTP_COMMAND_AUTH;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("ADAT", commandStr) == 0) {
		command->type = FTP_COMMAND_ADAT;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("MIC", commandStr) == 0) {
		command->type = FTP_COMMAND_MIC;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("CONF", commandStr) == 0) {
		command->type = FTP_COMMAND_CONF;
		MAKE_STRING_ARG(argumentStr);
		return command;
	} else if(strcmp("ENC", commandStr) == 0) {
		command->type = FTP_COMMAND_ENC;
		MAKE_STRING_ARG(argumentStr);
		return command;
	}

	// TODO: implement these
	//     TYPE <SP> <type-code> <CRLF>
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

	array->data = NULL;
	array->size = 0;

	const char* const separators = "\r\n";
	size_t separatorsLength = strlen(separators);

	int size_to_proccess = strlen(rawFtpCommands);
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

		int length = resultingIndex - currentlyAt;

		// overwrite this, so that this is the end of ths string
		*resultingIndex = '\0';

		FTPCommand* command = parseSingleFTPCommand(currentlyAt);

		if(!command) {
			freeFTPCommandArray(array);
			FREE_AT_END();
			return NULL;
		}

		array->size++;
		array->data = realloc(array->data, array->size);
		array->data[array->size - 1] = command;

		int actualLength = length + separatorsLength;
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
		default: break;
	}
}

void freeFTPCommandArray(FTPCommandArray* array) {
	for(size_t i = 0; i < array->size; ++i) {
		freeFTPCommand(array->data[i]);
	}

	free(array);
}
