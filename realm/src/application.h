#pragma once

#include "defines.h"
#include "event_handler.h"
#include "game.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#include "realm_app_loader.h"

typedef struct rl_application_config {
    const char *title;
    b8 vsync;
    RENDERER_BACKEND backend;

} rl_application_config;

typedef struct rl_application {
    rl_application_config config;
    platform_window window;
    rl_game *game_state;
    realm_app_context app_context;
    realm_app_module app_module;
    app_event_handler event_handler;
    b8 reload_requested;
} rl_application;

b8 create_application();
