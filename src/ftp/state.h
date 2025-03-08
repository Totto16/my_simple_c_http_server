

#pragma once

#include "./account.h"
#include <netinet/in.h>

/**
 * @enum MASK / FLAGS
 */
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

/**
 * @enum value
 */
typedef enum {
	FTP_MODE_STREAM = 0,
	// TODO: add rest

} FTP_MODE;

/**
 * @enum value
 */
typedef enum {
	FTP_STRUCTURE_FILE = 0,
	FTP_STRUCTURE_RECORD,
	// TODO: add rest

} FTP_STRUCTURE;

/**
 * @enum value
 */
typedef enum {
	FT_DATA_MODE_STANDARD = 0, // standard is what?
	FT_DATA_MODE_PASSIVE,
	FT_DATA_MODE_ACTIVE,
} FTPDataMode;

typedef struct sockaddr_in FTPConnectAddr;

typedef struct {
	FTPDataMode mode;
	FTPConnectAddr addr;
} FTPDataSettings;

typedef struct {
	const char* global_folder;
	AccountInfo* account;
	char* current_working_directory;
	FTP_TRANSMISSION_TYPE current_type;
	FTP_MODE mode;
	FTP_STRUCTURE structure;
	FTPDataSettings* data_settings;
} FTPState;

// see https://datatracker.ietf.org/doc/html/rfc959#section-5
FTPState* alloc_default_state(const char* global_folder, FTPConnectAddr addr);

// TODO: free state

char* get_current_dir_name(FTPState* state, bool escape);

char* make_address_port_desc(FTPConnectAddr addr);
