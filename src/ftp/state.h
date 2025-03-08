

#pragma once

#include "./account.h"

typedef enum {
	FTP_TRANSMISSION_TYPE_ASCII_NP = 0, // ASCII Non-print
	                                    // TODO: add rest

} FTP_TRANSMISSION_TYPE;

typedef enum {
	FTP_MODE_STREAM = 0,
	// TODO: add rest

} FTP_MODE;

typedef enum {
	FTP_STRUCTURE_FILE = 0,
	FTP_STRUCTURE_RECORD = 0,
	// TODO: add rest

} FTP_STRUCTURE;

typedef struct {
	const char* global_folder;
	AccountInfo* account;
	char* current_working_directory;
	FTP_TRANSMISSION_TYPE current_type;
	FTP_MODE mode;
	FTP_STRUCTURE structure;
} FTPState;

// see https://datatracker.ietf.org/doc/html/rfc959#section-5
FTPState* alloc_default_state(const char* global_folder);

// TODO: free state

char* get_current_dir_name(FTPState* state, bool escape);
