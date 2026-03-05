#pragma once

#include "defines.h"

#include "platform/platform.h"

typedef struct rl_clock {
    i64 frequency;
    i64 start;
    i64 last;
} rl_clock;

REALM_API void clock_reset(rl_clock *out_clock);
REALM_API void clock_update(rl_clock *clock);

REALM_API f64 clock_elapsed_s(rl_clock *clock);
