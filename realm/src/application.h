#pragma once

#include "defines.h"
#include "host/event_handler.h"
#include "host/host_cmd.h"
#include "platform/platform.h"

#include "module/realm_app_watcher.h"
#include "module/realm_app_loader.h"
#include "gui/app_console.h"
#include "gui/app_debug_panel.h"

typedef struct rl_scene rl_scene;

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
    rl_scene *scene;
    b8 focused;
    host_cmd_queue cmds;
} rl_application;

b8 create_application(const char *project_path);
