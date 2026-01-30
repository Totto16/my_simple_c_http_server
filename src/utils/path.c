
#include "./path.h"

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
