#pragma once

#include "defines.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RL_CONFIG_FILENAME "config.toml"

typedef struct rl_config {
    i32 window_width;
    i32 window_height;
    i32 window_x;
    i32 window_y;
    PLATFORM_WINDOW_MODE window_mode;

    RENDERER_BACKEND renderer_backend;
    b8 vsync;

    LOG_LEVEL log_level;

    b8 loaded; // true if config was loaded from file (not just defaults)
} rl_config;

REALM_API u64 config_system_size(void);
REALM_API b8 config_system_start(void *memory);
REALM_API void config_system_shutdown(void);

REALM_API rl_config *config_get(void);
REALM_API rl_config config_defaults(void);

REALM_API void config_mark_dirty(void);
REALM_API void config_flush_if_dirty(f64 dt);
REALM_API b8 config_save(void);

#ifdef __cplusplus
}
#endif
