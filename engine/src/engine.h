#pragma once

#include "defines.h"
#include "core/camera.h"

#include "platform/platform.h"

REALM_API b8 create_engine();
REALM_API void destroy_engine();

f64 engine_begin_frame(void);
void engine_end_frame(void);

REALM_API b8 engine_renderer_init(platform_window *render_window, rl_camera *camera);