#pragma once

#include "defines.h"
#include "core/camera.h"

#include "platform/platform.h"
#include "renderer/renderer_types.h"

typedef struct engine_stats {
    u64 fps;
} engine_stats;

REALM_API b8 create_engine();
REALM_API void destroy_engine();
b8 engine_is_running(void);

b8 engine_begin_frame(f64 *out_dt);
void engine_end_frame(void);
engine_stats engine_get_stats(void);

REALM_API b8 engine_renderer_init(platform_window *render_window, rl_camera *camera, RENDERER_BACKEND backend, b8 vsync);