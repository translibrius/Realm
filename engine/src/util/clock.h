#pragma once

#include "defines.h"

#include "platform/platform.h"

typedef struct rl_clock {
    i64 frequency;
    i64 start;
    i64 last;
} rl_clock;

void clock_reset(rl_clock *out_clock);
void clock_update(rl_clock *clock);

f64 clock_elapsed_s(rl_clock *clock);