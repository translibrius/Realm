#include "application.h"

#include "gui/app_console.h"
#include "host/app_output.h"
#include "host/app_renderer.h"
#include "host/app_scene.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/project_assets.h"
#include "core/scene.h"
#include "engine.h"
#include "host/event_handler.h"
#include "gui/gui.h"
#include "gui/gui_input.h"
#include "host/host_bootstrap.h"
#include "realm_app_cmd.h"
#include "module/realm_app_hot_reload.h"
#include "module/realm_app_loader.h"
#include "module/realm_app_module.h"
#include "platform/platform.h"
#include "util/str.h"

static rl_application app;

static void app_drain_host_cmds(void) {
    b8 rebuild = false;
    b8 reload = false;
    RENDERER_BACKEND switch_backend = 0;
    b8 do_switch = false;

    for (u32 i = 0; i < app.cmds.count; i++) {
        host_cmd cmd = app.cmds.items[i];
        switch (cmd.type) {
            case HOST_CMD_REBUILD_MODULE: rebuild = true; break;
            case HOST_CMD_RELOAD_MODULE:  reload = true;  break;
            case HOST_CMD_SWITCH_BACKEND:
                do_switch = true;
                switch_backend = cmd.backend;
                break;
        }
    }
    app.cmds.count = 0;

    if (do_switch) {
        if (app_renderer_switch_backend(&app, switch_backend)) {
            reload = true;
        }
    }

    if (reload) {
        if (rebuild) {
            if (!realm_app_module_rebuild()) {
                RL_ERROR("App module rebuild failed");
                return;
            }
            realm_app_watcher_mark_clean(&app.app_watcher);
        }

        RL_INFO("Reloading app module...");
        if (!realm_app_module_reload(&app.app_module, &app.game_state, &app.game_state_size, &app.app_context)) {
            RL_ERROR("App module reload failed");
        } else {
            realm_app_watcher_mark_clean(&app.app_watcher);
            RL_INFO("App module reloaded");
        }
    }
}

b8 create_application(const char *project_path) {
    app.game_state = nullptr;
    app.game_state_size = 0;
    app.focused = true;
    app.cmds = (host_cmd_queue){0};

    // Derive asset root from executable location so it works inside .app bundles too
    char exe_dir[512];
    const char *asset_root = "../../../assets/";
    char asset_root_buf[512];
    if (platform_get_executable_dir(exe_dir, sizeof(exe_dir))) {
        cstr_format_buf(asset_root_buf, sizeof(asset_root_buf), "%s../../../assets/", exe_dir);
        asset_root = asset_root_buf;
    }

    host_bootstrap_result boot = host_bootstrap(asset_root, "Realm", "config.toml", false, 0);
    if (!boot.success) return false;
    app.window = boot.window;

    rl_project *proj = project_open(project_path);
    if (!proj) {
        RL_FATAL("Failed to open project at '%s'", project_path);
        return false;
    }
    RL_INFO("Opened project '%s' at %s", proj->name, proj->root_path);
    project_load_assets();

    app.scene = app_scene_load_default(proj);

    // Console registers events first so it can consume key/char input when visible
    app_console_init(&app.console);
    gui_input_init();
    app_debug_panel_init(&app.debug_panel);
    app_event_handler_init(&app.event_handler, &app);

    if (!app_module_create(&app)) {
        RL_ERROR("failed to initialize app module");
        return false;
    }

    if (!realm_app_watcher_start(&app.app_watcher, REALM_APP_MODULE_NAME)) {
        RL_WARN("failed to start app module watcher");
    }

    // Main loop — cursor state is applied from the previous frame's commands
    // so that input_update + pump see the correct mode before the module reads deltas.
    f64 dt = 0.0f;
    b8 prev_capture = false;
    b8 cursor_visible = false;
    while (rl_engine_is_running()) {

        // Apply cursor state from previous frame, before input_update + pump
        b8 capture = app.focused && !cursor_visible && !app.console.core.visible;
        if (capture != prev_capture) {
            platform_set_cursor_mode(app.app_context.window, capture ? CURSOR_MODE_HIDDEN : CURSOR_MODE_NORMAL);
            platform_set_raw_input(app.app_context.window, capture);
            prev_capture = capture;
        }

        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        app_hot_reload_poll(&app);

        if (!realm_app_module_is_loaded(&app.app_module)) {
            rl_engine_end_frame();
            app_drain_host_cmds();
            continue;
        }

        app.app_context.focused = app.focused;
        app.app_context.window_mode = app.window.settings.window_mode;

        realm_app_cmd_queue module_cmds = {0};
        app.app_module.update(app.game_state, &app.app_context, &module_cmds, dt);

        gui_layout_begin((f32)dt);
        app.app_module.render(app.game_state, &app.app_context, &module_cmds);

        b8 show_debug = false;
        for (u32 i = 0; i < module_cmds.count; i++) {
            if (module_cmds.items[i].type == REALM_APP_CMD_SHOW_DEBUG_PANEL) {
                show_debug = module_cmds.items[i].b;
            }
            if (module_cmds.items[i].type == REALM_APP_CMD_SET_CURSOR_VISIBLE) {
                cursor_visible = module_cmds.items[i].b;
            }
        }
        if (show_debug) {
            app_debug_panel_render(&app.debug_panel);
        }
        app_console_render(&app.console, (f32)dt);
        gui_layout_end();

        app_output_process(&app, &module_cmds);

        rl_engine_end_frame();
        app_drain_host_cmds();
    }

    realm_app_watcher_stop(&app.app_watcher);
    app_module_destroy(&app);
    app_console_shutdown(&app.console);
    if (app.scene) {
        scene_destroy(app.scene);
        app.scene = nullptr;
    }
    if (project_is_open()) {
        project_close();
    }
    rl_engine_destroy();

    return true;
}

