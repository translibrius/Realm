#pragma once

#include "defines.h"
#include "platform/platform.h"

typedef struct host_bootstrap_result {
    b8 success;
    platform_window window;
} host_bootstrap_result;

// Engine create → config → window create → config track →
// renderer init (GL fallback) → init_gui.
REALM_API host_bootstrap_result host_bootstrap(const char *asset_root, const char *window_title, const char *config_filename);
