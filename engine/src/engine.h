#pragma once

#include "defines.h"
#include "../../realm/src/application.h"

typedef struct engine_stats {
    u64 fps;
} engine_stats;

REALM_API b8 create_engine();
REALM_API void destroy_engine();
b8 engine_is_running(void);

b8 engine_begin_frame(f64 *out_dt);
void engine_end_frame(void);
engine_stats engine_get_stats(void);