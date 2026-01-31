

#pragma once

#include "./utils.h"

NODISCARD char* get_serve_folder(const char* folder_to_resolve);

NODISCARD bool file_is_absolute(const char* file);

NODISCARD void* read_entire_file(const char* file_path, size_t* out_len);
