

#pragma once
#include "utils/utils.h"

NODISCARD bool setup_sigpipe_signal_handler(void);

NODISCARD size_t get_active_cpu_cores(void);
