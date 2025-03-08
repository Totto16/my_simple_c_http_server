

#pragma once

#include "./account.h"

typedef enum {
	FTP_TRANSMISSION_TYPE_ASCII = 0x1,      // ASCII
	FTP_TRANSMISSION_TYPE_EBCDIC = 0x2,     // EBCDIC
	FTP_TRANSMISSION_TYPE_IMAGE = 0x4,      // Image
	FTP_TRANSMISSION_TYPE_LOCAL_BYTE = 0x8, // Local Byte
	// FLAGS
	FTP_TRANSMISSION_TYPE_FLAG_NP = 0x10,  // Non-print
	FTP_TRANSMISSION_TYPE_FLAG_TEL = 0x20, // Telnet
	FTP_TRANSMISSION_TYPE_FLAG_ASA = 0x40, // CARRIAGE CONTROL (ASA)
	// MASK
	FTP_TRANSMISSION_TYPE_MASK_LB = 0xFF00,
	FTP_TRANSMISSION_TYPE_MASK_BASE = 0x0F,
	FTP_TRANSMISSION_TYPE_MASK_EXT = 0xF0

} FTP_TRANSMISSION_TYPE;

typedef enum {
	FTP_MODE_STREAM = 0,
	// TODO: add rest

} FTP_MODE;

typedef enum {
	FTP_STRUCTURE_FILE = 0x1,
	FTP_STRUCTURE_RECORD = 0x2,
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
