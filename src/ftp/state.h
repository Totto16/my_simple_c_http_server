

#pragma once

#include "./account.h"
#include <netinet/in.h>

/**
 * @enum MASK / FLAGS
 */
typedef enum ENUM_IS_MASK C_23_NARROW_ENUM_TO(uint16_t){
	FtpTransmissionTypeNone = 0,
	//
	FtpTransmissionTypeAscii = 0x1,     // ASCII
	FtpTransmissionTypeEbcdic = 0x2,    // EBCDIC
	FtpTransmissionTypeImage = 0x4,     // Image
	FtpTransmissionTypeLocalByte = 0x8, // Local Byte
	// FLAGS
	FtpTransmissionTypeFlagNp = 0x10,  // Non-print
	FtpTransmissionTypeFlagTel = 0x20, // Telnet
	FtpTransmissionTypeFlagAsa = 0x40, // CARRIAGE CONTROL (ASA)
} FtpTransmissionType;

// MASKS
#define FtpTransmissionTypeMaskLb ((FtpTransmissionType)0xFF00)
#define FtpTransmissionTypeMaskBase ((FtpTransmissionType)0x0F)
#define FtpTransmissionTypeMaskExt ((FtpTransmissionType)0xF0)

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FtpModeStream = 0,
	FtpModeBlock,
	FtpModeCompressed,
} FtpMode;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FtpStructureFile = 0,
	FtpStructureRecord,
	FtpStructurePage
} FtpStructure;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	SendModeUnsupported = 0, // standard is what?
	SendModeStreamBinaryFile,
	SendModeStreamBinaryRecord,
} SendMode;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	FtpDataModeNone = 0, // standard is what?
	FtpDataModePassive,
	FtpDataModeActive,
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
	FtpTransmissionType current_type;
	FtpMode mode;
	FtpStructure structure;
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
