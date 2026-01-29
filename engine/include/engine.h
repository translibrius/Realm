#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rl_engine_stats {
    u64 fps;
} rl_engine_stats;

REALM_API b8 rl_engine_create(void);
REALM_API void rl_engine_destroy(void);
REALM_API b8 rl_engine_is_running(void);

REALM_API b8 rl_engine_begin_frame(f64 *out_dt);
REALM_API void rl_engine_end_frame(void);
REALM_API rl_engine_stats rl_engine_get_stats(void);

// Legacy API (temporary). Prefer rl_engine_*.
typedef rl_engine_stats engine_stats;
REALM_API b8 create_engine(void);
REALM_API void destroy_engine(void);
REALM_API b8 engine_is_running(void);
REALM_API b8 engine_begin_frame(f64 *out_dt);
REALM_API void engine_end_frame(void);
REALM_API engine_stats engine_get_stats(void);

#ifdef __cplusplus
}
#endif
