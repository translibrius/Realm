#pragma once

#include "defines.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

struct rl_application;

b8 app_window_create(platform_window *window, const platform_window_settings *settings_override);
b8 app_renderer_switch_backend(struct rl_application *application, RENDERER_BACKEND backend);
