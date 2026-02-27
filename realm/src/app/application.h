#pragma once

#include "defines.h"
#include "debug/debug_window.h"
#include "app/event_handler.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#include "hotreload/realm_app_watcher.h"
#include "hotreload/realm_app_loader.h"

typedef struct rl_application_config {
    const char *title;
    b8 vsync;
    RENDERER_BACKEND backend;

} rl_application_config;

typedef struct rl_application {
    rl_application_config config;
    platform_window window;
    void *game_state;
    u64 game_state_size;
    realm_app_context app_context;
    realm_app_module app_module;
    realm_app_watcher app_watcher;
    app_event_handler event_handler;
    b8 paused;
    b8 focused;
    b8 rebuild_requested;
    b8 reload_requested;
    b8 backend_switch_requested;
    RENDERER_BACKEND requested_backend;

    debug_window_state debug_window;
} rl_application;

b8 create_application();
b8 app_create_window(rl_application *app, const platform_window_settings *settings_override);
