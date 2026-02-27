
#include "app/application.h"

#include "app/app_module_mgr.h"
#include "app/app_toast.h"
#include "app/backend_switch.h"
#include "core/config.h"
#include "core/logger.h"
#include "debug/debug_window.h"
#include "engine.h"
#include "app/event_handler.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"

static rl_application_config config = {
    .title = "Realm",
    .vsync = false,
    .backend = BACKEND_OPENGL,
};

static rl_application app;

b8 create_application() {
    rl_engine_config engine_config = rl_engine_config_default();
    engine_config.asset_root = "../../../assets/";
    engine_config.log_level = LOG_TRACE;

    app.config = config;
    app.game_state = nullptr;
    app.game_state_size = 0;
    app.focused = true;
    app.paused = false;
    app.rebuild_requested = false;
    app.reload_requested = false;
    app.backend_switch_requested = false;
    app.requested_backend = app.config.backend;

    if (!rl_engine_create(&engine_config)) {
        RL_FATAL("Engine failed to bootstrap");
        return false;
    }

    // Apply persisted config
    rl_config *cfg = config_get();
    if (cfg) {
        app.config.backend = cfg->renderer_backend;
        app.config.vsync = cfg->vsync;
    }

    platform_window_settings win_settings = {
        .title = "Realm",
        .x = cfg ? cfg->window_x : 0,
        .y = cfg ? cfg->window_y : 0,
        .width = cfg ? cfg->window_width : 500,
        .height = cfg ? cfg->window_height : 500,
        .start_center = !cfg || !cfg->loaded,
        .window_flags = WINDOW_FLAG_DEFAULT,
        .window_mode = cfg ? cfg->window_mode : WINDOW_MODE_WINDOWED,
    };

    if (!app_create_window(&app, &win_settings)) {
        return false;
    }

    config_track_window(&app.window);
    app_event_handler_init(&app.event_handler, &app);

    if (!renderer_init(&app.window, app.config.backend, app.config.vsync)) {
        if (app.config.backend != BACKEND_OPENGL) {
            RL_WARN("Renderer init failed (backend=%d). Falling back to OpenGL.", app.config.backend);
            app.config.backend = BACKEND_OPENGL;
            if (!renderer_init(&app.window, app.config.backend, app.config.vsync)) {
                RL_ERROR("failed to initialize renderer");
                return false;
            }
        } else {
            RL_ERROR("failed to initialize renderer");
            return false;
        }
    }

    if (!app_module_create(&app)) {
        RL_ERROR("failed to initialize app module");
        return false;
    }

    if (!realm_app_watcher_start(&app.app_watcher, REALM_APP_MODULE_NAME)) {
        RL_WARN("failed to start app module watcher");
    }

    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        RL_PROFILE_FRAME_MARK();
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        if (realm_app_watcher_poll(&app.app_watcher)) {
            app.reload_requested = true;
            RL_INFO("Detected app module file change, scheduling reload");
            app_push_toast(&app, REALM_APP_TOAST_INFO, "Detected module change");
        }

        if (app.reload_requested) {
            app.reload_requested = false;

            if (app.rebuild_requested) {
                app.rebuild_requested = false;
                if (!realm_app_module_rebuild()) {
                    RL_ERROR("App module rebuild failed");
                    app_push_toast(&app, REALM_APP_TOAST_ERROR, "App module rebuild failed");
                    rl_engine_end_frame();
                    continue;
                }
                realm_app_watcher_mark_clean(&app.app_watcher);
            }

            RL_INFO("Reloading app module...");
            app_push_toast(&app, REALM_APP_TOAST_INFO, "Reloading app module...");
            if (!realm_app_module_reload(&app.app_module, &app.game_state, &app.game_state_size, &app.app_context)) {
                RL_ERROR("App module reload failed");
                app_push_toast(&app, REALM_APP_TOAST_ERROR, "App module reload failed");
            } else {
                if (app.app_module.set_focused) {
                    app.app_module.set_focused(app.game_state, app.focused);
                }
                if (app.app_module.set_paused) {
                    app.app_module.set_paused(app.game_state, app.paused);
                }
                realm_app_watcher_mark_clean(&app.app_watcher);
                app_push_toast(&app, REALM_APP_TOAST_INFO, "App module reloaded");
            }
        }

        if (!realm_app_module_is_loaded(&app.app_module)) {
            rl_engine_end_frame();
            continue;
        }

        app.app_module.update(app.game_state, &app.app_context, dt);
        app.app_module.render(app.game_state, &app.app_context);
        debug_window_tick(&app.debug_window);
        rl_engine_end_frame();

        if (app.debug_window.toggle_requested) {
            app.debug_window.toggle_requested = false;
            if (app.debug_window.open) {
                debug_window_close(&app.debug_window);
                app_push_toast(&app, REALM_APP_TOAST_INFO, "Debug window closed");
            } else {
                if (debug_window_open(&app.debug_window, &app.window, app.config.backend)) {
                    app_push_toast(&app, REALM_APP_TOAST_INFO, "Debug window opened");
                } else {
                    app_push_toast(&app, REALM_APP_TOAST_WARNING, "Debug window requires OpenGL");
                }
            }
        }

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            app_switch_renderer_backend(&app);
        }
    }

    realm_app_watcher_stop(&app.app_watcher);
    debug_window_close(&app.debug_window);
    app_module_destroy(&app);
    rl_engine_destroy();

    return true;
}

b8 app_create_window(rl_application *app, const platform_window_settings *settings_override) {
    if (settings_override) {
        app->window.settings = *settings_override;
    } else {
        app->window.settings = (platform_window_settings){
            .title = "Realm",
            .x = 0,
            .y = 0,
            .width = 500,
            .height = 500,
            .start_center = true,
            .window_flags = WINDOW_FLAG_DEFAULT,
            .window_mode = WINDOW_MODE_WINDOWED,
        };
    }

    if (!platform_create_window(&app->window)) {
        RL_ERROR("failed to create main window");
        return false;
    }

    return true;
}
