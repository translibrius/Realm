#pragma once

#include "core/logger.h"
#include "defines.h"
#include "memory/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rl_engine_stats {
    u64 fps;
    f64 frame_time_ms;
} rl_engine_stats;

typedef struct rl_engine_config {
    // Asset root directory (contains fonts/, shaders/, textures/).
    // Example: "../../../assets/" when launching from build/*/bin.
    const char *asset_root;
} rl_engine_config;

REALM_API rl_engine_config rl_engine_config_default(void);
REALM_API b8 rl_engine_create(const rl_engine_config *config);
REALM_API void rl_engine_destroy(void);
REALM_API b8 rl_engine_is_running(void);
REALM_API void rl_engine_stop(void);

REALM_API b8 rl_engine_begin_frame(f64 *out_dt);
REALM_API void rl_engine_end_frame(void);
REALM_API rl_engine_stats rl_engine_get_stats(void);

// Per-frame arena cleared at rl_engine_end_frame(). Use for temporary
// allocations that must survive until end of frame (e.g. GUI strings).
REALM_API rl_arena *rl_engine_get_frame_arena(void);

#ifdef __cplusplus
}
#endif
