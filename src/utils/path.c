
#include "./path.h"

#include "utils/log.h"

#include <cwalk.h>
#include <sys/stat.h>
#include <unistd.h>

NODISCARD tstr get_serve_folder(const tstr* const folder_to_resolve) {
	const char* const folder_impl = realpath(tstr_cstr(folder_to_resolve), NULL);

	if(folder_impl == NULL) {
		fprintf(stderr, "Couldn't resolve folder '" TSTR_FMT "': %s\n",
		        TSTR_FMT_ARGS(*folder_to_resolve), strerror(errno));
		return tstr_null();
	}

	const tstr folder = tstr_from(folder_impl);

	struct stat stat_result;
	const int result = // NOLINT(totto-use-fixed-width-types-var)
	    stat(tstr_cstr(&folder), &stat_result);

	if(result != 0) {
		fprintf(stderr, "Couldn't stat folder '" TSTR_FMT "': %s\n", TSTR_FMT_ARGS(folder),
		        strerror(errno));
		return tstr_null();
	}

	if(!(S_ISDIR(stat_result.st_mode))) {
		fprintf(stderr, "Folder '" TSTR_FMT "' is not a directory\n", TSTR_FMT_ARGS(folder));
		return tstr_null();
	}

	if(access(tstr_cstr(&folder), R_OK) != 0) {
		fprintf(stderr, "Can't read from folder '" TSTR_FMT "': %s\n", TSTR_FMT_ARGS(folder),
		        strerror(errno));
		return tstr_null();
	}

	return folder;
}

NODISCARD bool file_is_absolute(const char* const file) {
	if(strlen(file) == 0) {
		// NOTE: 0 length is the same as "." so it is not absolute
		return false;
	}

	// TODO(Totto): check or report if upstream supports empty value here
	return cwk_path_is_absolute(file);
}

NODISCARD bool get_file_size_of_file(const char* file_path, OUT_PARAM(size_t) out_len) {

	if(out_len == NULL) {
		return false;
	}

	FILE* file = fopen(file_path, "rb");

	if(file == NULL) {
		LOG_MESSAGE(LogLevelError, "Couldn't open file for reading '%s': %s\n", file_path,
		            strerror(errno));

		return false;
	}

	const int fseek_res = // NOLINT(totto-use-fixed-width-types-var)
	    fseek(file, 0, SEEK_END);

	if(fseek_res != 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't seek to end of file '%s': %s\n", file_path,
		            strerror(errno));

		return false;
	}

	const long file_size = // NOLINT(totto-use-fixed-width-types-var)
	    ftell(file);

	const int fclose_result = // NOLINT(totto-use-fixed-width-types-var)
	    fclose(file);

	if(fclose_result != 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't close file '%s': %s\n", file_path, strerror(errno));

		return false;
	}

	*out_len = file_size;
	return true;
}

NODISCARD void* read_entire_file(const char* file_path, OUT_PARAM(size_t) out_len) {

	if(out_len == NULL) {
		return NULL;
	}

	FILE* file = fopen(file_path, "rb");

	if(file == NULL) {
		LOG_MESSAGE(LogLevelError, "Couldn't open file for reading '%s': %s\n", file_path,
		            strerror(errno));

		return NULL;
	}

	const int fseek_res = // NOLINT(totto-use-fixed-width-types-var)
	    fseek(file, 0, SEEK_END);

	if(fseek_res != 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't seek to end of file '%s': %s\n", file_path,
		            strerror(errno));

		return NULL;
	}

	const long file_size = // NOLINT(totto-use-fixed-width-types-var)
	    ftell(file);

	const int fseek_res2 // NOLINT(totto-use-fixed-width-types-var)
	    = fseek(file, 0, SEEK_SET);

	if(fseek_res2 != 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't seek to end of file '%s': %s\n", file_path,
		            strerror(errno));

		return NULL;
	}

	uint8_t* file_data = (uint8_t*)malloc(file_size * sizeof(uint8_t));

	if(!file_data) {

		fclose(file);
		return NULL;
	}

	const size_t fread_result = fread(file_data, 1, file_size, file);

	if(fread_result != (size_t)file_size) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't read the correct amount of bytes from file '%s': %s\n",
		            file_path, strerror(errno));

		fclose(file);
		free(file_data);
		return NULL;
	}

	const int fclose_result = // NOLINT(totto-use-fixed-width-types-var)
	    fclose(file);

	if(fclose_result != 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't close file '%s': %s\n", file_path, strerror(errno));

		free(file_data);
		return NULL;
	}

	*out_len = file_size;
	return file_data;
}
