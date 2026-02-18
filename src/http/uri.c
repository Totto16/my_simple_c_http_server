
#include "./uri.h"

#include "utils/string_builder.h"

TMAP_IMPLEMENT_MAP_TYPE(char*, CHAR_PTR_KEYNAME, ParsedSearchPathValue, ParsedSearchPathHashMap)

NODISCARD ParsedURLPath parse_url_path(char* const path) {
	// precodnition:  path is not NULL and len is > 1

	char* search_path = strchr(path, '?');

	ParsedURLPath result = { .search_path = {
		                         .hash_map = TMAP_EMPTY(ParsedSearchPathHashMap),
		                     } ,.fragment = NULL};

	if(search_path == NULL) {
		result.path = strdup(path);

		return result;
	}

	*search_path = '\0';

	result.path = strdup(path);

	char* search_params = search_path + 1;

	char* fragment_part = strchr(search_params, '#');

	if(fragment_part != NULL) {
		*fragment_part = '\0';

		result.fragment = strdup(fragment_part + 1);
	}

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

		const TmapInsertResult insert_result = TMAP_INSERT(
		    ParsedSearchPathHashMap, &(result.search_path.hash_map), key_dup, value_entry, false);

		switch(insert_result) {
			case TmapInsertResultWouldOverwrite: {
				// TODO: if this header has to be unique, error, if this header can be
				// concatenatend, like e.g. cookie, concatene it, otherwise i don't know what to do
				UNREACHABLE();
				break;
			}
			case TmapInsertResultOk: {
				break;
			}
			case TmapInsertResultErr:
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

NODISCARD static URIUserInfo parse_user_info(char* const userinfo) {

	URIUserInfo result = { .username = NULL, .password = NULL };

	char* password_part = strchr(userinfo, ':');

	if(password_part != NULL) {
		*password_part = '\0';

		char* password_str = password_part + 1;

		result.password = strdup(password_str);
	}

	result.username = strdup(userinfo);

	return result;
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

NODISCARD static uint16_t parse_u16(const char* to_parse, OUT_PARAM(bool) success) {

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

NODISCARD char* parse_authority(char* const str, OUT_PARAM(ParsedAuthority) out_result) {

	ParsedAuthority local_authority = {
		.user_info = (URIUserInfo){ .username = NULL, .password = NULL }, .host = NULL, .port = 0
	};

	char* current_ptr = str;

	char* userinfo_part = strchr(current_ptr, '@');

	if(userinfo_part != NULL) {

		*userinfo_part = '\0';

		local_authority.user_info = parse_user_info(str);

		current_ptr = userinfo_part + 1;
	}

	char* port_part = strchr(current_ptr, ':');

	char* authority_end = NULL;

	if(port_part != NULL) {
		*port_part = '\0';
		local_authority.host = strdup(current_ptr);

		char* const port_str = port_part + 1;

		authority_end = find_authority_end(port_str);

		assert(authority_end != NULL);

		bool success = false;

		char* valid_port_str = strndup(port_str, authority_end - port_str);

		uint16_t port = parse_u16(valid_port_str, &success);

		free(valid_port_str);

		if(!success && port == 0) {
			free(local_authority.host);
			return NULL;
		}

		local_authority.port = port;
	} else {

		authority_end = find_authority_end(current_ptr);

		assert(authority_end != NULL);

		local_authority.host = strndup(current_ptr, authority_end - current_ptr);
		local_authority.port = 0;
	}

	*out_result = local_authority;

	return authority_end;
}

#define SCHEME_SEPERATOR "://"

#define SIZEOF_SCHEME_SEPERATOR 3

static_assert((sizeof(SCHEME_SEPERATOR) / (sizeof(SCHEME_SEPERATOR[0]))) - 1 ==
              SIZEOF_SCHEME_SEPERATOR);

NODISCARD static ParsedRequestURIResult parse_uri_or_authority(char* const path) {
	// precodnition:  path is not NULL and len is > 1

	ParsedRequestURI result = {};

	char* scheme_identifier = strstr(path, SCHEME_SEPERATOR);

	if(scheme_identifier == NULL) {
		// parse authority

		ParsedAuthority authority = {};
		char* authority_parse_result = parse_authority(path, &authority);

		if(authority_parse_result == NULL) {

			return (ParsedRequestURIResult){
				.is_error = true,
				.value = { .error = "Authority parse error: not a valid authority" }
			};
		}

		if(*authority_parse_result != '\0') {

			return (ParsedRequestURIResult){ .is_error = true,
				                             .value = {
				                                 .error = "Authority parse error: we got more data "
				                                          "after the authority, but no scheme "
				                                          "(<scheme>://), so this is a URI missing "
				                                          "that scheme or an invalid authority" } };
		}

		result.type = ParsedURITypeAuthority;
		result.data.authority = authority;

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	ParsedURI uri = {};

	*scheme_identifier = '\0';

	uri.scheme = strdup(path);

	char* current_ptr = scheme_identifier + SIZEOF_SCHEME_SEPERATOR;

	// TODO: note, some uris might not have an authority, is that even valid? maybe with file://
	// /test/hello.txt, but otherwise?

	ParsedAuthority authority = {};
	char* authority_parse_result = parse_authority(current_ptr, &authority);

	if(authority_parse_result == NULL) {
		return (ParsedRequestURIResult){
			.is_error = true, .value = { .error = "Authority parse error: not a valid authority" }
		};
	}

	uri.authority = authority;

	if(*authority_parse_result == '\0') {
		// the path is empty

		ParsedURLPath parsed_path =  {.path=strdup("/"), .search_path= {
		                         .hash_map = TMAP_INIT(ParsedSearchPathHashMap),
		                     },.fragment = NULL};

		uri.path = parsed_path;
	} else {
		// the path is not empty

		ParsedURLPath parsed_path = parse_url_path(authority_parse_result);
		uri.path = parsed_path;
	}

	result.type = ParsedURITypeAbsoluteURI;
	result.data.uri = uri;

	return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
}

NODISCARD ParsedRequestURIResult parse_request_uri(char* const path) {

	ParsedRequestURI result = {};

	if(path == NULL || strlen(path) == 0) {
		result.type = ParsedURITypeAbsPath;
		result.data.path = (ParsedURLPath){.path=strdup("/"), .search_path= {
		                         .hash_map = TMAP_INIT(ParsedSearchPathHashMap),
		                     },.fragment = NULL};

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	if(strlen(path) == 1 && path[0] == '*') {
		result.type = ParsedURITypeAsterisk;

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	if(path[0] == '/') {

		ParsedURLPath parsed_path = parse_url_path(path);

		result.type = ParsedURITypeAbsPath;
		result.data.path = parsed_path;

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	return parse_uri_or_authority(path);
}

static void free_parsed_url_path(ParsedURLPath path) {
	free(path.path);

	TMAP_TYPENAME_ITER(ParsedSearchPathHashMap)
	iter = TMAP_ITER_INIT(ParsedSearchPathHashMap, &path.search_path.hash_map);

	TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) value;

	while(TMAP_ITER_NEXT(ParsedSearchPathHashMap, &iter, &value)) {
		free(value.key);
		free(value.value.value);
	}

	TMAP_FREE(ParsedSearchPathHashMap, &(path.search_path.hash_map));

	if(path.fragment) {
		free(path.fragment);
	}
}

static void free_user_info(URIUserInfo user_info) {
	// NOTE. it is not necessary to check for NULL, but i do it for good measures
	if(user_info.username) {
		free(user_info.username);
	}

	if(user_info.password) {
		free(user_info.password);
	}
}

static void free_parsed_authority(ParsedAuthority authority) {

	free_user_info(authority.user_info);
	free(authority.host);
}

static void free_parsed_uri(ParsedURI uri) {

	free(uri.scheme);
	free_parsed_authority(uri.authority);
	free_parsed_url_path(uri.path);
}

void free_parsed_request_uri(ParsedRequestURI uri) {

	switch(uri.type) {
		case ParsedURITypeAsterisk: {
			break;
		}
		case ParsedURITypeAbsoluteURI: {
			free_parsed_uri(uri.data.uri);
			break;
		}
		case ParsedURITypeAbsPath: {
			free_parsed_url_path(uri.data.path);
			break;
		}
		case ParsedURITypeAuthority: {
			free_parsed_authority(uri.data.authority);
			break;
		}
		default: {
			break;
		}
	}
}

NODISCARD char* get_parsed_url_as_string(ParsedURLPath path) {

	// TODO: support escape codes!

	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, path.path);

	if(!TMAP_IS_EMPTY(ParsedSearchPathHashMap, &path.search_path.hash_map)) {

		string_builder_append_single(string_builder, "?");

		TMAP_TYPENAME_ITER(ParsedSearchPathHashMap)
		iter = TMAP_ITER_INIT(ParsedSearchPathHashMap, &path.search_path.hash_map);

		TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) value;

		bool start = true;

		while(TMAP_ITER_NEXT(ParsedSearchPathHashMap, &iter, &value)) {

			if(!start) {
				string_builder_append_single(string_builder, "&");
			}

			start = true;

			string_builder_append_single(string_builder, value.key);

			if(value.value.value != NULL && strlen(value.value.value) != 0) {
				string_builder_append_single(string_builder, "=");

				string_builder_append_single(string_builder, value.value.value);
			}
		}
	}

	return string_builder_release_into_string(&string_builder);
}

NODISCARD char* get_parsed_authority_as_string(ParsedAuthority authority) {

	StringBuilder* string_builder = string_builder_init();

	if(authority.user_info.username != NULL) {
		string_builder_append_single(string_builder, authority.user_info.username);
		if(authority.user_info.password != NULL) {
			string_builder_append_single(string_builder, ":");
			string_builder_append_single(string_builder, authority.user_info.password);
		}
		string_builder_append_single(string_builder, "@");
	}

	string_builder_append_single(string_builder, authority.host);

	if(authority.port != 0) {
		STRING_BUILDER_APPENDF(string_builder, return NULL;, ":%u", authority.port);
	}

	return string_builder_release_into_string(&string_builder);
}

NODISCARD char* get_uri_as_string(ParsedURI uri) {
	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, uri.scheme);

	string_builder_append_single(string_builder, "://");

	if(uri.authority.host != NULL) {
		char* authority_str = get_parsed_authority_as_string(uri.authority);
		string_builder_append_single(string_builder, authority_str);
		free(authority_str);
	}

	char* path_str = get_parsed_url_as_string(uri.path);
	string_builder_append_single(string_builder, path_str);
	free(path_str);

	return string_builder_release_into_string(&string_builder);
}

NODISCARD char* get_request_uri_as_string(ParsedRequestURI uri) {

	switch(uri.type) {
		case ParsedURITypeAsterisk: {
			return strdup("*");
		}
		case ParsedURITypeAbsoluteURI: {
			return get_uri_as_string(uri.data.uri);
		}
		case ParsedURITypeAbsPath: {
			return get_parsed_url_as_string(uri.data.path);
		}
		case ParsedURITypeAuthority: {
			return get_parsed_authority_as_string(uri.data.authority);
		}
		default: {
			return NULL;
		}
	}
}

static ParsedAuthority duplicate_authority(const ParsedAuthority authority) {

	ParsedAuthority result = { .user_info = { .username = NULL, .password = NULL },
		                       .host = NULL,
		                       .port = 0 };

	if(authority.user_info.username != NULL) {
		result.user_info.username = strdup(authority.user_info.username);
	}

	if(authority.user_info.password != NULL) {
		result.user_info.password = strdup(authority.user_info.password);
	}

	if(authority.host != NULL) {
		result.host = strdup(authority.host);
	}

	result.port = authority.port;

	return result;
}

static ParsedURLPath duplicate_path(const ParsedURLPath path) {

	ParsedURLPath result = {
		.path = NULL,
		.search_path = { .hash_map = TMAP_EMPTY(ParsedSearchPathHashMap), }
		,.fragment = NULL,
	};

	if(path.path != NULL) {
		result.path = strdup(path.path);
	}

	TMAP_TYPENAME_ITER(ParsedSearchPathHashMap)
	iter = TMAP_ITER_INIT(ParsedSearchPathHashMap, &path.search_path.hash_map);

	TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) value;

	while(TMAP_ITER_NEXT(ParsedSearchPathHashMap, &iter, &value)) {

		char* value_dup = strdup(value.value.value);
		char* key_dup = strdup(value.key);

		const ParsedSearchPathValue value_entry = { .value = value_dup };

		const TmapInsertResult insert_result = TMAP_INSERT(
		    ParsedSearchPathHashMap, &(result.search_path.hash_map), key_dup, value_entry, false);

		assert(insert_result == TmapInsertResultOk);
	}

	if(path.fragment != NULL) {
		result.fragment = strdup(path.fragment);
	}

	return result;
}

static ParsedURI duplicate_uri(const ParsedURI uri) {

	ParsedURI result = { .scheme = NULL,
		                 .authority = { .user_info = { .username = NULL, .password = NULL },
		                                .host = NULL,
		                                .port = 0 },
		                 .path = {
		                     .path = NULL,
		                     .search_path = { .hash_map = TMAP_EMPTY(ParsedSearchPathHashMap) },
		                     .fragment = NULL } };

	if(uri.scheme != NULL) {
		result.scheme = strdup(uri.scheme);
	}

	result.authority = duplicate_authority(uri.authority);

	result.path = duplicate_path(uri.path);

	return result;
}

NODISCARD ParsedRequestURI duplicate_request_uri(const ParsedRequestURI uri) {

	switch(uri.type) {
		case ParsedURITypeAsterisk: {
			return uri;
		}
		case ParsedURITypeAbsoluteURI: {
			return (ParsedRequestURI){ .type = ParsedURITypeAbsoluteURI,
				                       .data = { .uri = duplicate_uri(uri.data.uri) } };
		}
		case ParsedURITypeAbsPath: {
			return (ParsedRequestURI){ .type = ParsedURITypeAbsPath,
				                       .data = { .path = duplicate_path(uri.data.path) } };
		}
		case ParsedURITypeAuthority: {
			return (ParsedRequestURI){ .type = ParsedURITypeAuthority,
				                       .data = { .authority =
				                                     duplicate_authority(uri.data.authority) } };
		}
		default: {
			UNREACHABLE();
		}
	}
}
