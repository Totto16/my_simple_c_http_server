
#pragma once

#include "./state.h"

NODISCARD char* get_dir_name_relative_to_ftp_root(FTPState* state, char* file, bool escape);

NODISCARD char* get_current_dir_name(FTPState* state, bool escape);

NODISCARD char* resolve_path_in_cwd(FTPState* state, char* file);
