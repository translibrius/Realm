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
#include "module/realm_app_hot_reload.h"
#include "module/realm_app_module.h"
#include "platform/platform.h"
#include "util/str.h"

static rl_application app;

b8 create_application(const char *project_path) {
    app.game_state = nullptr;
    app.game_state_size = 0;
    app.focused = true;
    app.rebuild_requested = false;
    app.reload_requested = false;
    app.backend_switch_requested = false;
    app.requested_backend = BACKEND_OPENGL;

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

    // Main loop — cursor state is applied from the previous frame's output
    // so that input_update + pump see the correct mode before the module reads deltas.
    f64 dt = 0.0f;
    b8 prev_capture = false;
    realm_app_output prev_output = {0};
    while (rl_engine_is_running()) {

        // Apply cursor state from previous frame's module output, before input_update + pump
        b8 capture = app.focused && !prev_output.wants_cursor_visible && !app.console.core.visible;
        if (capture != prev_capture) {
            platform_set_cursor_mode(app.app_context.window, capture ? CURSOR_MODE_HIDDEN : CURSOR_MODE_NORMAL);
            platform_set_raw_input(app.app_context.window, capture);
            prev_capture = capture;
        }

        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        if (!app_hot_reload_tick(&app)) {
            rl_engine_end_frame();
            continue;
        }

        if (!realm_app_module_is_loaded(&app.app_module)) {
            rl_engine_end_frame();
            continue;
        }

        app.app_context.focused = app.focused;
        app.app_context.window_mode = app.window.settings.window_mode;

        realm_app_output module_output = {0};
        app.app_module.update(app.game_state, &app.app_context, &module_output, dt);

        gui_layout_begin((f32)dt);
        app.app_module.render(app.game_state, &app.app_context, &module_output);
        if (module_output.show_debug_panel) {
            app_debug_panel_render(&app.debug_panel);
        }
        app_console_render(&app.console, (f32)dt);
        gui_layout_end();

        app_output_process(&app, &module_output);

        prev_output = module_output;
        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            app_renderer_switch_backend(&app, app.requested_backend);
        }
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

