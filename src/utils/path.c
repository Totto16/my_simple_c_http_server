
#include "./path.h"

#include "utils/log.h"

#include <cwalk.h>
#include <sys/stat.h>
#include <unistd.h>

NODISCARD char* get_serve_folder(const char* folder_to_resolve) {
	char* folder = realpath(folder_to_resolve, NULL);

	if(folder == NULL) {
		fprintf(stderr, "Couldn't resolve folder '%s': %s\n", folder_to_resolve, strerror(errno));
		return NULL;
	}

	struct stat stat_result;
	int result = stat(folder, &stat_result);

	if(result != 0) {
		fprintf(stderr, "Couldn't stat folder '%s': %s\n", folder, strerror(errno));
		return NULL;
	}

	if(!(S_ISDIR(stat_result.st_mode))) {
		fprintf(stderr, "Folder '%s' is not a directory\n", folder);
		return NULL;
	}

	if(access(folder, R_OK) != 0) {
		fprintf(stderr, "Can't read from folder '%s': %s\n", folder, strerror(errno));
		return NULL;
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

	int fseek_res = fseek(file, 0, SEEK_END);

	if(fseek_res != 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't seek to end of file '%s': %s\n", file_path,
		            strerror(errno));

		return NULL;
	}

	long file_size = ftell(file);
	fseek_res = fseek(file, 0, SEEK_SET);

	if(fseek_res != 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't seek to end of file '%s': %s\n", file_path,
		            strerror(errno));

		return NULL;
	}

	uint8_t* file_data = (uint8_t*)malloc(file_size * sizeof(uint8_t));

	if(!file_data) {

		fclose(file);
		return NULL;
	}

	size_t fread_result = fread(file_data, 1, file_size, file);

	if(fread_result != (size_t)file_size) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't read the correct amount of bytes from file '%s': %s\n",
		            file_path, strerror(errno));

		fclose(file);
		free(file_data);
		return NULL;
	}

	int fclose_result = fclose(file);

	if(fclose_result != 0) {
		LOG_MESSAGE(LogLevelWarn, "Couldn't close file '%s': %s\n", file_path, strerror(errno));

		free(file_data);
		return NULL;
	}

	*out_len = file_size;
	return file_data;
}
