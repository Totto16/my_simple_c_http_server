
#pragma once

#include "./state.h"
#include "generic/secure.h"

// opaque type
typedef struct SendDataImpl SendData;

typedef struct SendProgressImpl SendProgress;

/**
 * @enum value
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	DirChangeResultOk = 0,
	DirChangeResultNoSuchDir,
	DirChangeResultError,
	DirChangeResultErrorPathTraversal,
} DirChangeResult;

NODISCARD char* get_dir_name_relative_to_ftp_root(const FTPState* state, const char* file,
                                                  bool escape);

NODISCARD char* get_current_dir_name_relative_to_ftp_root(const FTPState* state, bool escape);

NODISCARD char* resolve_path_in_cwd(const FTPState* state, const char* file);

NODISCARD DirChangeResult change_dirname_to(FTPState* state, const char* file);

NODISCARD SendProgress* setup_send_progress(const SendData* data, SendMode send_mode);

NODISCARD bool send_progress_is_finished(SendProgress* progress);

NODISCARD SendData* get_data_to_send_for_list(bool is_folder, char* path, FileSendFormat format);

NODISCARD SendData* get_data_to_send_for_retr(char* path);

NODISCARD bool send_data_to_send(const SendData* data, ConnectionDescriptor* descriptor,
                                 SendMode send_mode, SendProgress* progress);

void free_send_data(SendData* data);

void free_send_progress(SendProgress* progress);

// NOTE: this overwrites files
NODISCARD bool write_to_file(char* path, void* data, size_t data_size);
