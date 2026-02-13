#pragma once

#include "core/logger.h"
#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rl_engine_stats {
    u64 fps;
} rl_engine_stats;

typedef struct rl_engine_config {
    // Asset root directory (contains fonts/, shaders/, textures/).
    // Example: "../../../assets/" when launching from build/*/bin.
    const char *asset_root;
    LOG_LEVEL log_level;
} rl_engine_config;

REALM_API rl_engine_config rl_engine_config_default(void);
REALM_API b8 rl_engine_create(const rl_engine_config *config);
REALM_API void rl_engine_destroy(void);
REALM_API b8 rl_engine_is_running(void);
REALM_API void rl_engine_stop(void);

REALM_API b8 rl_engine_begin_frame(f64 *out_dt);
REALM_API void rl_engine_end_frame(void);
REALM_API rl_engine_stats rl_engine_get_stats(void);

#ifdef __cplusplus
}
#endif
