#pragma once

#include <stdlib.h>
#include "platform/platform.h"

#include "defines.h"

i64 rand_int_range(i64 from, i64 to) {
    static bool seeded = false;
    if (!seeded) {
        srand(platform_get_absolute_time());
        seeded = true;
    }

    return rand() % (to - from + 1) + from;
}

f64 rand_float01() {
    return rand() / (float)RAND_MAX;
}
