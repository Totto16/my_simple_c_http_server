

#pragma once

#include "./account.h"
#include <netinet/in.h>

/**
 * @enum MASK / FLAGS
 */
typedef enum C_23_NARROW_ENUM_TO(uint16_t) {
	FTP_TRANSMISSION_TYPE_NONE = 0,
	//
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
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FTP_MODE_STREAM = 0,
	FTP_MODE_BLOCK,
	FTP_MODE_COMPRESSED,

} FTP_MODE;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FTP_STRUCTURE_FILE = 0,
	FTP_STRUCTURE_RECORD,
	FTP_STRUCTURE_PAGE

} FTP_STRUCTURE;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	SEND_MODE_UNSUPPORTED = 0, // standard is what?
	SEND_MODE_STREAM_BINARY_FILE,
	SEND_MODE_STREAM_BINARY_RECORD,
} SendMode;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FTP_DATA_MODE_NONE = 0, // standard is what?
	FTP_DATA_MODE_PASSIVE,
	FTP_DATA_MODE_ACTIVE,
} FTPDataMode;

typedef uint32_t FTPAddrField;
typedef uint16_t FTPPortField;

/**
 * @brief everything here is little endian (so you have to use conversion functions to use it e.g.
 * htons for the port)
 *
 */
typedef struct {
	FTPAddrField addr;
	FTPPortField port;
} FTPPortInformation;

typedef FTPPortInformation FTPConnectAddr;

typedef struct {
	FTPDataMode mode;
	// in passive mode this is the data_addr (of the server), in active mode the port that was given
	// to us, in standard mode this is undefined
	FTPConnectAddr addr;
} FTPDataSettings;

typedef struct {
	char* name;
	char* arguments;
} FTPSupportedFeature;

typedef struct {
	FTPSupportedFeature* features;
	size_t size;
} FTPSupportedFeatures;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FileSendFormatLs = 0,
	FileSendFormatEplf,
} FileSendFormat;

typedef struct {
	FileSendFormat send_format;
} CustomFTPOptions;

typedef struct {
	const char* global_folder;
	AccountInfo* account;
	char* current_working_directory;
	FTP_TRANSMISSION_TYPE current_type;
	FTP_MODE mode;
	FTP_STRUCTURE structure;
	FTPDataSettings* data_settings;
	FTPSupportedFeatures* supported_features;
	CustomFTPOptions* options;
} FTPState;

// see https://datatracker.ietf.org/doc/html/rfc959#section-5
NODISCARD FTPState* alloc_default_state(const char* global_folder);

void free_state(FTPState* state);

NODISCARD char* make_address_port_desc(FTPConnectAddr addr);

NODISCARD FTPPortInformation get_port_info_from_sockaddr(struct sockaddr_in addr);

NODISCARD SendMode get_current_send_mode(FTPState* state);
