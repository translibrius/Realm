#pragma once

#include "defines.h"
#include "event_handler.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#include "realm_app_watcher.h"
#include "realm_app_loader.h"
#include "app_console.h"
#include "app_debug_panel.h"

typedef struct rl_application {
    platform_window window;
    void *game_state;
    u64 game_state_size;
    realm_app_context app_context;
    realm_app_module app_module;
    realm_app_watcher app_watcher;
    app_event_handler event_handler;
    app_console console;
    app_debug_panel debug_panel;
    b8 paused;
    b8 focused;
    b8 rebuild_requested;
    b8 reload_requested;
    b8 backend_switch_requested;
    RENDERER_BACKEND requested_backend;
} rl_application;

b8 create_application(void);
