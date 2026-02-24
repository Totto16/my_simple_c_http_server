

#pragma once

#include "utils/utils.h"
#include <tmap.h>
#include <tstr.h>

typedef struct {
	tstr /* NULLABLE */ username;
	tstr /* NULLABLE */ password;
} URIUserInfo;

typedef struct {
	URIUserInfo /* NULLABLE */ user_info;
	tstr host;
	uint16_t /* NULLABLE */ port;
} ParsedAuthority;

typedef struct {
	tstr val;
} ParsedSearchPathValue;

TMAP_DEFINE_MAP_TYPE(tstr, TSTR_KEYNAME, ParsedSearchPathValue, ParsedSearchPathHashMap)

typedef TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) ParsedSearchPathEntry;

typedef struct {
	TMAP_TYPENAME_MAP(ParsedSearchPathHashMap) hash_map;
} ParsedSearchPath;

// RFC: https://datatracker.ietf.org/doc/html/rfc1738
typedef struct {
	tstr path;
	ParsedSearchPath /* NULLABLE */ search_path;
	tstr /* NULLABLE */ fragment;
} ParsedURLPath;

//  URI spec: https://datatracker.ietf.org/doc/html/rfc3986
typedef struct {
	tstr scheme;
	ParsedAuthority authority;
	ParsedURLPath path;
} ParsedURI;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	ParsedURITypeAsterisk = 0,
	ParsedURITypeAbsoluteURI,
	ParsedURITypeAbsPath,
	ParsedURITypeAuthority,
} ParsedURIType;

// spec:
// Request-URI    = "*" | absoluteURI | abs_path | authority
typedef struct {
	ParsedURIType type;
	union {
		ParsedURI uri;
		ParsedURLPath path;
		ParsedAuthority authority;
	} data;
} ParsedRequestURI;

typedef struct {
	bool is_error;
	union {
		const char* error;
		ParsedRequestURI uri;
	} value;
} ParsedRequestURIResult;

NODISCARD ParsedURLPath parse_url_path(tstr_view path);

typedef struct {
	ParsedAuthority authority;
	tstr_view after;
	bool ok;
} AuthorityResult;

NODISCARD AuthorityResult parse_authority(tstr_view str);

/**
 * @brief Get the parsed url path from raw object, it modifies the string inline and creates
 * copies for the result
 *
 * @param path
 * @return ParsedRequestURIResult
 */
NODISCARD ParsedRequestURIResult parse_request_uri(tstr_view path);

void free_parsed_request_uri(ParsedRequestURI uri);

NODISCARD tstr get_parsed_url_as_string(ParsedURLPath path);

NODISCARD tstr get_parsed_authority_as_string(ParsedAuthority authority);

NODISCARD tstr get_uri_as_string(ParsedURI uri);

NODISCARD tstr get_request_uri_as_string(ParsedRequestURI uri);

NODISCARD ParsedRequestURI duplicate_request_uri(ParsedRequestURI uri);
