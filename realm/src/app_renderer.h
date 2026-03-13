#pragma once

#include "defines.h"
#include "renderer/renderer_backend.h"

struct rl_application;

b8 app_renderer_switch_backend(struct rl_application *application, RENDERER_BACKEND backend);
