#include "application.h"

#include "app_console.h"
#include "app_renderer.h"
#include "core/config.h"
#include "core/logger.h"
#include "engine.h"
#include "event_handler.h"
#include "gui/gui.h"
#include "memory/memory.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"

static rl_application app;

static b8 create_app_module(void);
static void destroy_app_module(void);

b8 create_application(void) {
    rl_engine_config engine_config = rl_engine_config_default();
    engine_config.asset_root = "../../../assets/";

    app.game_state = nullptr;
    app.game_state_size = 0;
    app.focused = true;
    app.rebuild_requested = false;
    app.reload_requested = false;
    app.backend_switch_requested = false;
    app.requested_backend = BACKEND_OPENGL;

    if (!rl_engine_create(&engine_config)) {
        RL_FATAL("Engine failed to bootstrap");
        return false;
    }

    rl_config *cfg = config_get();
    platform_window_settings win_settings = config_to_window_settings(cfg, "Realm");

    if (!app_window_create(&app.window, &win_settings)) {
        return false;
    }

    config_track_window(&app.window);

    if (!renderer_init(&app.window, cfg->renderer_backend, cfg->vsync)) {
        if (cfg->renderer_backend != BACKEND_OPENGL) {
            RL_WARN("Renderer init failed (backend=%d). Falling back to OpenGL.", cfg->renderer_backend);
            cfg->renderer_backend = BACKEND_OPENGL;
            config_mark_dirty();
            if (!renderer_init(&app.window, cfg->renderer_backend, cfg->vsync)) {
                RL_ERROR("failed to initialize renderer");
                return false;
            }
        } else {
            RL_ERROR("failed to initialize renderer");
            return false;
        }
    }

    init_gui((f32)app.window.settings.width, (f32)app.window.settings.height);
    // Console registers events first so it can consume key/char input when visible
    app_console_init(&app.console);
    app_debug_panel_init(&app.debug_panel);
    app_event_handler_init(&app.event_handler, &app);

    if (!create_app_module()) {
        RL_ERROR("failed to initialize app module");
        return false;
    }

    if (!realm_app_watcher_start(&app.app_watcher, REALM_APP_MODULE_NAME)) {
        RL_WARN("failed to start app module watcher");
    }

    rl_profiler_init();

    // Main loop — cursor state is applied from the previous frame's output
    // so that input_update + pump see the correct mode before the module reads deltas.
    f64 dt = 0.0f;
    b8 prev_capture = false;
    realm_app_output prev_output = {0};
    while (rl_engine_is_running()) {
        rl_profiler_frame_mark();

        // Apply cursor state from previous frame's module output, before input_update + pump
        b8 capture = app.focused && !prev_output.wants_cursor_visible && !app.console.window.visible;
        if (capture != prev_capture) {
            platform_set_cursor_mode(app.app_context.window, capture ? CURSOR_MODE_HIDDEN : CURSOR_MODE_NORMAL);
            platform_set_raw_input(app.app_context.window, capture);
            prev_capture = capture;
        }

        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        if (realm_app_watcher_poll(&app.app_watcher)) {
            app.reload_requested = true;
            RL_INFO("Detected app module file change, scheduling reload");
        }

        if (app.reload_requested) {
            app.reload_requested = false;

            if (app.rebuild_requested) {
                app.rebuild_requested = false;
                if (!realm_app_module_rebuild()) {
                    RL_ERROR("App module rebuild failed");
                    rl_engine_end_frame();
                    continue;
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

        // Process module requests
        if (module_output.wants_quit) {
            rl_engine_stop();
        }
        if (module_output.wants_vsync_change) {
            config_set_vsync(module_output.vsync_value);
            app.app_context.vsync = config_get()->vsync;
        }
        if (module_output.wants_window_mode_change) {
            platform_set_window_mode(&app.window, module_output.window_mode_value);
        }
        if (module_output.wants_fov_change) {
            config_set_fov(module_output.fov_value);
            app.app_context.fov = config_get()->fov;
        }
        if (module_output.wants_sensitivity_change) {
            config_set_mouse_sensitivity(module_output.sensitivity_value);
            app.app_context.mouse_sensitivity = config_get()->mouse_sensitivity;
        }
        if (module_output.wants_backend_switch) {
            app.backend_switch_requested = true;
            app.requested_backend = module_output.requested_backend;
        }

        prev_output = module_output;
        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            app_renderer_switch_backend(&app, app.requested_backend);
        }
    }

    realm_app_watcher_stop(&app.app_watcher);
    destroy_app_module();
    app_console_shutdown(&app.console);
    rl_profiler_write_session_report("profiler_session.bin");
    rl_profiler_shutdown();
    rl_engine_destroy();

    return true;
}

// App module glue

static b8 create_app_module(void) {
    if (!realm_app_module_load(&app.app_module)) {
        RL_ERROR("failed to load app module");
        return false;
    }

    app.game_state_size = app.app_module.get_state_size();
    if (app.game_state_size < sizeof(u32)) {
        RL_ERROR("app module state size is invalid: %llu", app.game_state_size);
        realm_app_module_unload(&app.app_module);
        return false;
    }

    app.game_state = mem_alloc(app.game_state_size, MEM_APPLICATION);
    if (!app.game_state) {
        RL_ERROR("failed to allocate app state");
        realm_app_module_unload(&app.app_module);
        return false;
    }
    mem_zero(app.game_state, app.game_state_size);

    app.app_context = (realm_app_context){
        .window = &app.window,
        .vsync = config_get()->vsync,
        .focused = app.focused,
        .renderer_backend = config_get()->renderer_backend,
        .window_mode = config_get()->window_mode,
        .fov = config_get()->fov,
        .mouse_sensitivity = config_get()->mouse_sensitivity,
    };

    app.app_module.init(app.game_state, &app.app_context);
    RL_INFO("App module initialized");
    return true;
}

static void destroy_app_module(void) {
    if (realm_app_module_is_loaded(&app.app_module)) {
        app.app_module.shutdown(app.game_state, &app.app_context);
    }
    if (app.game_state) {
        mem_free(app.game_state, app.game_state_size, MEM_APPLICATION);
    }
    app.game_state = nullptr;
    app.game_state_size = 0;
    realm_app_module_unload(&app.app_module);
}
