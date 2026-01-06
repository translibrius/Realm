#pragma once

#include "defines.h"
#include "core/camera.h"

#include "platform/platform.h"

REALM_API b8 create_engine();
REALM_API void destroy_engine();

REALM_API b8 engine_renderer_init(platform_window *render_window, rl_camera *camera);

REALM_API b8 engine_run();