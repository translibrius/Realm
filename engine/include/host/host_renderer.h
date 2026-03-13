#pragma once

#include "defines.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

typedef struct host_switch_result {
    b8 success;
    b8 rolled_back;
    RENDERER_BACKEND active_backend;
} host_switch_result;

// Create a window with either provided settings or sensible defaults.
REALM_API b8 host_window_create(platform_window *window,
                                const platform_window_settings *settings_override,
                                const char *default_title);

// Destroy renderer + window, recreate window, init new backend, rollback on failure.
// Updates config + GUI dimensions on success. Caller handles app-specific state.
REALM_API host_switch_result host_renderer_switch_backend(platform_window *window,
                                                          RENDERER_BACKEND new_backend,
                                                          const char *window_title);
