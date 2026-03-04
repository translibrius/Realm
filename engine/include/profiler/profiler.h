#pragma once

#include "defines.h"

// Exclude a function from -finstrument-functions instrumentation.
// Usage: RL_PROFILE_SKIP void my_hot_helper(void) { ... }
#if defined(__clang__) || defined(__GNUC__)
#define RL_PROFILE_SKIP __attribute__((no_instrument_function))
#else
#define RL_PROFILE_SKIP
#endif

typedef struct rl_profile_zone {
    const char *name;
    void       *fn_addr;
    u32         call_count;
    i64         total_ns;
    i64         max_ns;
    i64         avg_ns;
} rl_profile_zone;

typedef struct rl_profile_frame {
    rl_profile_zone *zones;       // sorted by total_ns descending
    u32              zone_count;
    i64              frame_time_ns;
} rl_profile_frame;

#ifndef RL_PROFILE_ENABLED
#define RL_PROFILE_ENABLED 0
#endif

#if RL_PROFILE_ENABLED

REALM_API void                    rl_profiler_init(void);
REALM_API void                    rl_profiler_shutdown(void);
REALM_API void                    rl_profiler_frame_mark(void);
REALM_API const rl_profile_frame *rl_profiler_get_frame(void);
REALM_API void                    rl_profiler_set_enabled(b8 enabled);
REALM_API b8                      rl_profiler_is_enabled(void);
REALM_API void                    rl_profiler_write_session_report(const char *path);

#else

#define rl_profiler_init()                  ((void)0)
#define rl_profiler_shutdown()              ((void)0)
#define rl_profiler_frame_mark()            ((void)0)
#define rl_profiler_get_frame()             ((const rl_profile_frame *)0)
#define rl_profiler_set_enabled(e)          ((void)0)
#define rl_profiler_is_enabled()            ((b8)0)
#define rl_profiler_write_session_report(p) ((void)0)

#endif
