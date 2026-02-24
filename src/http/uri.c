
#include "./uri.h"

#include "utils/string_builder.h"

TMAP_IMPLEMENT_MAP_TYPE(tstr, TSTR_KEYNAME, ParsedSearchPathValue, ParsedSearchPathHashMap)

NODISCARD ParsedURLPath parse_url_path(const tstr_view path) {
	// precondition:  path is not NULL and len is > 1

	const tstr_split_result search_path_res = tstr_split(path, "?");

	ParsedURLPath result = { .search_path = {
		                         .hash_map = TMAP_EMPTY(ParsedSearchPathHashMap),
		                     } ,.fragment = tstr_init()};

	tstr_view search_path;
	tstr_view path_view;
	tstr_view fragment_view;

	if(!search_path_res.ok) {
		search_path = TSTR_EMPTY_VIEW;

		const tstr_split_result fragment_res = tstr_split(path, "#");

		if(!fragment_res.ok) {
			path_view = path;
			fragment_view = TSTR_EMPTY_VIEW;
		} else {
			path_view = fragment_res.first;
			fragment_view = fragment_res.second;
		}

	} else {
		path_view = search_path_res.first;

		const tstr_split_result fragment_res = tstr_split(search_path_res.second, "#");

		if(!fragment_res.ok) {
			search_path = search_path_res.second;
			fragment_view = TSTR_EMPTY_VIEW;
		} else {
			search_path = fragment_res.first;
			fragment_view = fragment_res.second;
		}
	}

	result.path = tstr_from_view(path_view);
	result.fragment = tstr_from_view(fragment_view);

	tstr_view search_params = search_path;

	if(search_params.len == 0) {
		return result;
	}

	while(search_params.len != 0) {

		const tstr_split_result next_arg_res = tstr_split(search_params, "&");

		tstr_view current_value;

		if(!next_arg_res.ok) {
			current_value = search_params;
			search_params = TSTR_EMPTY_VIEW;
		} else {
			current_value = next_arg_res.first;
			search_params = next_arg_res.second;
		}

		const tstr_split_result value_res = tstr_split(current_value, "=");

		tstr_view value;
		tstr_view key;

		if(!value_res.ok) {
			key = current_value;
			value = TSTR_EMPTY_VIEW;
		} else {
			key = value_res.first;
			value = value_res.second;
		}

		const ParsedSearchPathValue value_entry = { .val = tstr_from_view(value) };

		const TmapInsertResult insert_result =
		    TMAP_INSERT(ParsedSearchPathHashMap, &(result.search_path.hash_map),
		                tstr_from_view(key), value_entry, false);

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
	}

	return result;
}

NODISCARD static URIUserInfo parse_user_info(const tstr_view userinfo) {

	URIUserInfo result = { .username = tstr_init(), .password = tstr_init() };

	const tstr_split_result password_res = tstr_split(userinfo, ":");

	if(!password_res.ok) {
		result.username = tstr_from_view(userinfo);
	} else {
		result.username = tstr_from_view(password_res.first);
		result.password = tstr_from_view(password_res.second);
	}

	return result;
}

typedef struct {
	tstr_view authority;
	tstr_view after;
} AuthoritySplit;

NODISCARD static AuthoritySplit find_authority_split(const tstr_view str) {

	// from the spec:   The authority component is [...] and is
	// terminated by the next slash ("/"), question mark ("?"), or number
	// sign ("#") character, or by the end of the URI.
	// see: https://datatracker.ietf.org/doc/html/rfc3986#section-3.2

	for(size_t i = 0; i < str.len; ++i) {

		const char val = str.data[i];

		if(val == '/' || val == '?' || val == '#') {
			// NOTE: the current char is not excluded, as it belong to the after part
			const tstr_view authority = { .data = str.data, .len = i };
			const tstr_view after = { .data = str.data + i, .len = str.len - i };

			return (AuthoritySplit){ .authority = authority, .after = after };
		}
	}

	return (AuthoritySplit){ .authority = str, .after = TSTR_EMPTY_VIEW };
}

NODISCARD static uint16_t parse_u16(const tstr_view to_parse, OUT_PARAM(bool) success) {

	int result = 0;
	const bool succ = tstr_view_to_int(to_parse, &result);

	if(!succ) {
		*success = false;
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

NODISCARD AuthorityResult parse_authority(const tstr_view str) {

	// see: https://datatracker.ietf.org/doc/html/rfc3986#section-3.2

	ParsedAuthority authority = { .user_info = (URIUserInfo){ .username = tstr_init(),
		                                                      .password = tstr_init() },
		                          .host = tstr_init(),
		                          .port = 0 };

	tstr_view current_view = str;

	const tstr_split_result userinfo_part = tstr_split(current_view, "@");

	if(userinfo_part.ok) {
		authority.user_info = parse_user_info(userinfo_part.first);

		current_view = userinfo_part.second;
	}

	const tstr_split_result port_part = tstr_split(current_view, ":");

	tstr_view authority_after;

	if(port_part.ok) {
		authority.host = tstr_from_view(port_part.first);

		AuthoritySplit authority_split = find_authority_split(port_part.second);

		bool success = false;

		uint16_t port = parse_u16(authority_split.authority, &success);

		if(!success && port == 0) {
			tstr_free(&authority.host);
			return (AuthorityResult){ .ok = false };
		}

		authority.port = port;
		authority_after = authority_split.after;
	} else {

		AuthoritySplit authority_split = find_authority_split(current_view);

		authority.host = tstr_from_view(authority_split.authority);
		authority.port = 0;
		authority_after = authority_split.after;
	}

	return (AuthorityResult){ .ok = true, .authority = authority, .after = authority_after };
}

#define SCHEME_SEPERATOR "://"

NODISCARD static ParsedRequestURIResult parse_uri_or_authority(const tstr_view path) {
	// precodnition:  path is not NULL and len is > 1

	ParsedRequestURI result = {};

	const tstr_split_result scheme_identifier = tstr_split(path, SCHEME_SEPERATOR);

	if(!scheme_identifier.ok) {
		// parse authority

		AuthorityResult authority_parse_result = parse_authority(path);

		if(!authority_parse_result.ok) {

			return (ParsedRequestURIResult){
				.is_error = true,
				.value = { .error = "Authority parse error: not a valid authority" }
			};
		}

		if(authority_parse_result.after.len != 0) {

			return (ParsedRequestURIResult){ .is_error = true,
				                             .value = {
				                                 .error = "Authority parse error: we got more data "
				                                          "after the authority, but no scheme "
				                                          "(<scheme>://), so this is a URI missing "
				                                          "that scheme or an invalid authority" } };
		}

		result.type = ParsedURITypeAuthority;
		result.data.authority = authority_parse_result.authority;

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	ParsedURI uri = {};

	// see: https://datatracker.ietf.org/doc/html/rfc3986#section-3.1
	uri.scheme = tstr_from_view(scheme_identifier.first);

	const tstr_view current_view = scheme_identifier.second;

	// TODO: note, some uris might not have an authority, is that even valid? maybe with file://
	// /test/hello.txt, but otherwise?

	AuthorityResult authority_parse_result = parse_authority(current_view);

	if(!authority_parse_result.ok) {
		return (ParsedRequestURIResult){
			.is_error = true, .value = { .error = "Authority parse error: not a valid authority" }
		};
	}

	uri.authority = authority_parse_result.authority;

	const tstr_view after_authority = authority_parse_result.after;

	if(after_authority.len == 0) {
		// the path is empty

		ParsedURLPath parsed_path =  {.path=tstr_from("/"), .search_path= {
		                         .hash_map = TMAP_INIT(ParsedSearchPathHashMap),
		                     },.fragment = tstr_init()};

		uri.path = parsed_path;
	} else {
		// the path is not empty

		ParsedURLPath parsed_path = parse_url_path(after_authority);
		uri.path = parsed_path;
	}

	result.type = ParsedURITypeAbsoluteURI;
	result.data.uri = uri;

	return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
}

NODISCARD ParsedRequestURIResult parse_request_uri(const tstr_view path) {
	// see https://datatracker.ietf.org/doc/html/rfc3986#section-3

	ParsedRequestURI result = {};

	if(path.len == 0) {
		result.type = ParsedURITypeAbsPath;
		result.data.path = (ParsedURLPath){.path=tstr_from("/"), .search_path= {
		                         .hash_map = TMAP_INIT(ParsedSearchPathHashMap),
		                     },.fragment = tstr_init()};

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	if(path.len == 1 && path.data[0] == '*') {
		result.type = ParsedURITypeAsterisk;

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	if(path.data[0] == '/') {

		ParsedURLPath parsed_path = parse_url_path(path);

		result.type = ParsedURITypeAbsPath;
		result.data.path = parsed_path;

		return (ParsedRequestURIResult){ .is_error = false, .value = { .uri = result } };
	}

	return parse_uri_or_authority(path);
}

static void free_parsed_url_path(ParsedURLPath path) {
	tstr_free(&path.path);

	TMAP_TYPENAME_ITER(ParsedSearchPathHashMap)
	iter = TMAP_ITER_INIT(ParsedSearchPathHashMap, &path.search_path.hash_map);

	TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) value;

	while(TMAP_ITER_NEXT(ParsedSearchPathHashMap, &iter, &value)) {
		tstr_free(&value.key);
		tstr_free(&value.value.val);
	}

	TMAP_FREE(ParsedSearchPathHashMap, &(path.search_path.hash_map));

	tstr_free(&path.fragment);
}

static void free_user_info(URIUserInfo user_info) {
	tstr_free(&user_info.username);

	tstr_free(&user_info.password);
}

static void free_parsed_authority(ParsedAuthority authority) {
	free_user_info(authority.user_info);
	tstr_free(&authority.host);
}

static void free_parsed_uri(ParsedURI uri) {
	tstr_free(&uri.scheme);
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

NODISCARD tstr get_parsed_url_as_string(ParsedURLPath path) {

	// TODO: support escape codes!

	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, tstr_cstr(&path.path));

	if(!TMAP_IS_EMPTY(ParsedSearchPathHashMap, &path.search_path.hash_map)) {

		string_builder_append_single(string_builder, "?");

		TMAP_TYPENAME_ITER(ParsedSearchPathHashMap)
		iter = TMAP_ITER_INIT(ParsedSearchPathHashMap, &path.search_path.hash_map);

		TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) entry;

		bool start = true;

		while(TMAP_ITER_NEXT(ParsedSearchPathHashMap, &iter, &entry)) {

			if(!start) {
				string_builder_append_single(string_builder, "&");
			}

			start = true;

			string_builder_append_single(string_builder, tstr_cstr(&entry.key));

			if(tstr_len(&entry.value.val) != 0) {
				string_builder_append_single(string_builder, "=");

				string_builder_append_single(string_builder, tstr_cstr(&entry.value.val));
			}
		}
	}

	SizedBuffer result = string_builder_release_into_sized_buffer(&string_builder);

	return tstr_own(result.data, result.size, result.size);
}

NODISCARD tstr get_parsed_authority_as_string(ParsedAuthority authority) {

	StringBuilder* string_builder = string_builder_init();

	if(tstr_cstr(&authority.user_info.username) != NULL) {
		string_builder_append_single(string_builder, tstr_cstr(&authority.user_info.username));
		if(tstr_cstr(&authority.user_info.password) != NULL) {
			string_builder_append_single(string_builder, ":");
			string_builder_append_single(string_builder, tstr_cstr(&authority.user_info.password));
		}
		string_builder_append_single(string_builder, "@");
	}

	string_builder_append_single(string_builder, tstr_cstr(&authority.host));

	if(authority.port != 0) {
		STRING_BUILDER_APPENDF(string_builder, return tstr_init();, ":%u", authority.port);
	}

	SizedBuffer result = string_builder_release_into_sized_buffer(&string_builder);

	return tstr_own(result.data, result.size, result.size);
}

NODISCARD tstr get_uri_as_string(ParsedURI uri) {
	StringBuilder* string_builder = string_builder_init();

	string_builder_append_single(string_builder, tstr_cstr(&uri.scheme));

	string_builder_append_single(string_builder, "://");

	if(tstr_cstr(&uri.authority.host) != NULL) {
		tstr authority_str = get_parsed_authority_as_string(uri.authority);
		string_builder_append_single(string_builder, tstr_cstr(&authority_str));
		tstr_free(&authority_str);
	}

	tstr path_str = get_parsed_url_as_string(uri.path);
	string_builder_append_single(string_builder, tstr_cstr(&path_str));
	tstr_free(&path_str);

	SizedBuffer result = string_builder_release_into_sized_buffer(&string_builder);

	return tstr_own(result.data, result.size, result.size);
}

NODISCARD tstr get_request_uri_as_string(ParsedRequestURI uri) {

	switch(uri.type) {
		case ParsedURITypeAsterisk: {
			return tstr_from("*");
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
			return tstr_init();
		}
	}
}

static ParsedAuthority duplicate_authority(const ParsedAuthority authority) {

	ParsedAuthority result = { .user_info = { .username = tstr_init(), .password = tstr_init() },
		                       .host = tstr_init(),
		                       .port = 0 };

	if(tstr_cstr(&authority.user_info.username) != NULL) {
		result.user_info.username = tstr_dup(&authority.user_info.username);
	}

	if(tstr_cstr(&authority.user_info.password) != NULL) {
		result.user_info.password = tstr_dup(&authority.user_info.password);
	}

	if(tstr_cstr(&authority.host) != NULL) {
		result.host = tstr_dup(&authority.host);
	}

	result.port = authority.port;

	return result;
}

static ParsedURLPath duplicate_path(const ParsedURLPath path) {

	ParsedURLPath result = {
		.path = tstr_init(),
		.search_path = { .hash_map = TMAP_EMPTY(ParsedSearchPathHashMap), },
		.fragment = tstr_init(),
	};

	if(tstr_cstr(&path.path) != NULL) {
		result.path = tstr_dup(&path.path);
	}

	TMAP_TYPENAME_ITER(ParsedSearchPathHashMap)
	iter = TMAP_ITER_INIT(ParsedSearchPathHashMap, &path.search_path.hash_map);

	TMAP_TYPENAME_ENTRY(ParsedSearchPathHashMap) value;

	while(TMAP_ITER_NEXT(ParsedSearchPathHashMap, &iter, &value)) {

		tstr value_dup = tstr_dup(&value.value.val);
		tstr key_dup = tstr_dup(&value.key);

		const ParsedSearchPathValue value_entry = { .val = value_dup };

		const TmapInsertResult insert_result = TMAP_INSERT(
		    ParsedSearchPathHashMap, &(result.search_path.hash_map), key_dup, value_entry, false);

		assert(insert_result == TmapInsertResultOk);
	}

	if(tstr_cstr(&path.fragment) != NULL) {
		result.fragment = tstr_dup(&path.fragment);
	}

	return result;
}

static ParsedURI duplicate_uri(const ParsedURI uri) {

	ParsedURI result = {
		.scheme = tstr_init(),
		.authority = { .user_info = { .username = tstr_init(), .password = tstr_init() },
		               .host = tstr_init(),
		               .port = 0 },
		.path = { .path = tstr_init(),
		          .search_path = { .hash_map = TMAP_EMPTY(ParsedSearchPathHashMap) },
		          .fragment = tstr_init() }
	};

	if(tstr_cstr(&uri.scheme) != NULL) {
		result.scheme = tstr_dup(&uri.scheme);
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
