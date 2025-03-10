
#pragma once

#include "./state.h"
#include "generic/secure.h"

// opaque type
typedef struct SendDataImpl SendData;

typedef struct {
	size_t total_count;
	size_t sent_count;
	// TODO: support records, so  that we can keep track of the records we sent!
} SendProgressImplData;
typedef struct {
	bool finished;
	SendProgressImplData _impl;
} SendProgress;

NODISCARD char* get_dir_name_relative_to_ftp_root(const FTPState* state, const char* file,
                                                  bool escape);

NODISCARD char* get_current_dir_name(const FTPState* state, bool escape);

NODISCARD char* resolve_path_in_cwd(const FTPState* state, const char* file);

NODISCARD SendProgress setup_send_progress(const SendData* data, SendMode send_mode);

NODISCARD SendData* get_data_to_send_for_list(bool is_folder, const char* path);

NODISCARD bool send_data_to_send(const SendData* data, ConnectionDescriptor*  descriptor, SendMode send_mode,
                                 SendProgress* progress);

void free_send_data(SendData* data);
