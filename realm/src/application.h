#pragma once

#include "defines.h"
#include "game.h"
#include "platform/platform.h"
#include "core/camera.h"
#include "renderer/renderer_types.h"

typedef struct application_config {
    const char *title;
    b8 vsync;
    RENDERER_BACKEND backend;
} application_config;

typedef struct application {
    application_config config;
    platform_window main_window;
    rl_camera camera;
    game game_inst;
} application;

b8 create_application(application *app);