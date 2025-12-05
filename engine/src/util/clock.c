#include "clock.h"

void clock_reset(rl_clock *out_clock) {
    out_clock->frequency = platform_get_clock_frequency();
    out_clock->start = platform_get_clock_counter();
}

void clock_update(rl_clock *clock) {
    clock->last = platform_get_clock_counter();
}

// Returns elapsed time in seconds
f64 clock_elapsed_s(rl_clock *clock) {
    return (f64)(clock->last - clock->start) / clock->frequency;
}