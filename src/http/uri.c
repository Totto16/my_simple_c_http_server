
#include "./uri.h"

ZMAP_IMPLEMENT_MAP_TYPE(char*, CHAR_PTR_KEYNAME, ParsedSearchPathValue, ParsedSearchPathHashMap)

NODISCARD static ParsedURLPath get_parsed_url_path_from_raw(const char* path) {
	// precodnition:  path is not NULL and len is > 1

	const char* search_path = strchr(path, '?');

	ParsedURLPath result = { .search_path = {
		                         .hash_map = ZMAP_INIT(ParsedSearchPathHashMap),
		                     } };

	if(search_path == NULL) {
		result.path = strdup(path);

		return result;
	}

	*search_path = '\0';

	result.path = strdup(path);

	char* search_params = search_path + 1;

	if(strlen(search_params) == 0) {
		return result;
	}

	while(true) {

		char* next_argument = strchr(search_params, '&');

		if(next_argument != NULL) {
			*next_argument = '\0';
		}

		char* key = search_params;

		char* value_ptr = strchr(search_params, '=');

		if(value_ptr != NULL) {
			*value_ptr = '\0';
		}

		const char* value = value_ptr == NULL ? "" : value_ptr + 1;

		char* key_dup = strdup(key);
		char* value_dup = strdup(value);
		const ParsedSearchPathValue value_entry = { .value = value_dup };

		const ZmapInsertResult insert_result = ZMAP_INSERT(
		    ParsedSearchPathHashMap, &(result.search_path.hash_map), key_dup, value_entry, false);

		switch(insert_result) {
			case ZmapInsertResultWouldOverwrite: {
				// TODO: if this header has to be unique, error, if this header can be
				// concatenatend, like e.g. cookie, concatene it, otherwise i don't know what to do
				UNREACHABLE();
				break;
			}
			case ZmapInsertResultOk: {
				break;
			}
			case ZmapInsertResultErr:
			default: {
				// TODO: allow error!
				// return NULL;
				UNREACHABLE();
			}
		}

		if(next_argument == NULL) {
			break;
		}

		search_params = next_argument + 1;
	}

	return result;
}

NODISCARD static UserInfo parse_user_info(const char* userinfo) {

	char* password_part = strchr(userinfo, ':');

	if(password_part == NULL) {
	}
}

NODISCARD static char* find_authority_end(char* const str) {

	// from the spec:   The authority component is [...] and is
	// terminated by the next slash ("/"), question mark ("?"), or number
	// sign ("#") character, or by the end of the URI.

	char* end = str;

	while(true) {

		const char val = *end;

		if(val == '/' || val == '?' || val == '#' || val == '\0') {
			break;
		}

		end = end + 1;
	}

	return end;
}

NODISCARD static uint16_t parse_u16(const char* to_parse, bool* success) {

	long result = parse_long(to_parse, success);

	if(!(*success)) {
		return 0;
	}

	if(result < 0) {
		*success = false;
		return 0;
	}

	if(result > UINT16_MAX) {
		*success = false;
		return 0;
	}

	*success = true;
	return (uint16_t)result;
}

NODISCARD static char* parse_authority(char* const str, ParsedAuthority* out_result) {

	ParsedAuthority local_authority = {
		.user_info = (UserInfo){ .username = NULL, .password = NULL }, .host = NULL, .port = 0
	};

	char* current_ptr = str;

	char* userinfo_part = strchr(current_ptr, '@');

	if(userinfo_part != NULL) {

		*userinfo_part = '\0';

		local_authority.user_info = parse_user_info(str);

		current_ptr = userinfo_part + 1;
	}

	char* port_part = strchr(current_ptr, ':');

	char* at_or_after_end_ptr = NULL;

	if(port_part != NULL) {
		*port_part = '\0';
		local_authority.host = strdup(current_ptr);

		char* const port_str = port_part + 1;

		char* authority_end = find_authority_end(port_str);

		if(*authority_end == '\0') {
			at_or_after_end_ptr = authority_end; // no +1 as this is the end of the string!
		} else {
			*authority_end = '\0';
			at_or_after_end_ptr = authority_end + 1;
		}

		bool success = false;

		uint16_t port = parse_u16(port_str, &success);

		if(!success && port == 0) {
			free(local_authority.host);
			return NULL;
		}

		local_authority.port = port;
	} else {

		char* authority_end = find_authority_end(current_ptr);

		if(*authority_end == '\0') {
			at_or_after_end_ptr = authority_end; // no +1 as this is the end of the string!
		} else {
			*authority_end = '\0';
			at_or_after_end_ptr = authority_end + 1;
		}

		local_authority.host = strdup(current_ptr);
		local_authority.port = 0;
	}

	*out_result = local_authority;

	return at_or_after_end_ptr;
}

NODISCARD static ParsedRequestURI get_parsed_uri_or_authority_from_raw(char* const path) {
	// precodnition:  path is not NULL and len is > 1

	ParsedRequestURI result = {};

	char* scheme_identifier = strstr(path, "://");

	if(scheme_identifier == NULL) {
		// parse authority

		ParsedAuthority authority = {};
		char* authority_parse_result = parse_authority(path, &authority);

		if(authority_parse_result == NULL) {
			result.type = ParsedURITypeError;
			result.data.error = "Authority parse error: not a valid authority";

			return result;
		}

		if(*authority_parse_result != '\0') {
			result.type = ParsedURITypeError;
			result.data.error =
			    "Authority parse error: we got more data after the authority, but no scheme "
			    "(<scheme>://), so this is a URI missing that scheme or an invalid authority";

			return result;
		}

		result.type = ParsedURITypeAuthority;
		result.data.authority = authority;

		return result;
	}

	ParsedURI uri = {};

	*scheme_identifier = '\0';

	uri.scheme = strdup(path);

	char* current_ptr = scheme_identifier + 1;

	// TODO: note, some uris might not have an authority, is that even valid? maybe with file://
	// /test/hello.txt, but otherwise?

	ParsedAuthority authority = {};
	char* authority_parse_result = parse_authority(current_ptr, &authority);

	if(authority_parse_result == NULL) {
		result.type = ParsedURITypeError;
		result.data.error = "Authority parse error: not a valid authority";

		return result;
	}

	uri.authority = authority;

	if(*authority_parse_result == '\0') {
		// the path is not empty

		ParsedURLPath parsed_path = get_parsed_url_path_from_raw(path);
		uri.path = parsed_path;
	} else {
		// the path is empty

		ParsedURLPath parsed_path =  {.path=strdup("/"), .search_path= {
		                         .hash_map = ZMAP_INIT(ParsedSearchPathHashMap),
		                     },.fragment = NULL};

		uri.path = parsed_path;
	}

	result.type = ParsedURITypeAbsoluteURI;
	result.data.uri = uri;

	return result;
}

NODISCARD ParsedRequestURI get_parsed_request_uri_from_raw(char* const path) {

	ParsedRequestURI result = {};

	if(path == NULL || strlen(path) == 0) {
		result.type = ParsedURITypeAbsPath;
		result.data.path = (ParsedURLPath){.path=strdup("/"), .search_path= {
		                         .hash_map = ZMAP_INIT(ParsedSearchPathHashMap),
		                     },.fragment = NULL};

		return result;
	}

	if(strlen(path) == 1 && path[0] == '*') {
		result.type = ParsedURITypeAsterisk;

		return result;
	}

	if(path[0] == '/') {

		ParsedURLPath parsed_path = get_parsed_url_path_from_raw(path);

		result.type = ParsedURITypeAbsPath;
		result.data.path = parsed_path;

		return result;
	}

	result = get_parsed_uri_or_authority_from_raw(path);

	return result;
}
