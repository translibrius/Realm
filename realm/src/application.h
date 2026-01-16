#pragma once

#include "defines.h"
#include "renderer/renderer_types.h"
#include "game.h"

typedef struct rl_application_config {
    const char *title;
    b8 vsync;
    RENDERER_BACKEND backend;

} rl_application_config;

typedef struct rl_application {
    rl_application_config config;
    rl_game game;
    platform_window window;
} rl_application;

b8 create_application();