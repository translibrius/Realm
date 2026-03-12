#include "ed_application.h"

#include "core/config.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

static ed_application app;

static b8 ed_window_create(platform_window *window, const platform_window_settings *settings) {
    if (!window) return false;
    if (settings) {
        window->settings = *settings;
    } else {
        window->settings = (platform_window_settings){
            .title = "Realm Editor",
            .x = 0, .y = 0,
            .width = 1280, .height = 720,
            .start_center = true,
            .window_flags = WINDOW_FLAG_DEFAULT,
            .window_mode = WINDOW_MODE_WINDOWED,
        };
    }
    if (!platform_create_window(window)) {
        RL_ERROR("Failed to create editor window");
        return false;
    }
    return true;
}

static b8 ed_switch_backend(RENDERER_BACKEND backend) {
    rl_config *pcfg = config_get();
    const RENDERER_BACKEND previous_backend = pcfg->renderer_backend;
    const platform_window_settings previous_settings = app.window.settings;

    RL_INFO("Switching renderer backend: %d -> %d", previous_backend, backend);

    renderer_destroy();
    platform_destroy_window(app.window.id);

    if (!ed_window_create(&app.window, &previous_settings)) {
        RL_ERROR("Failed to recreate window for renderer switch");
        rl_engine_stop();
        return false;
    }

    if (!renderer_init(&app.window, backend, pcfg->vsync)) {
        RL_ERROR("Failed to initialize renderer backend %d, attempting rollback", backend);

        renderer_destroy();
        platform_destroy_window(app.window.id);

        if (!ed_window_create(&app.window, &previous_settings)) {
            RL_FATAL("Failed to recreate window while rolling back renderer backend");
            rl_engine_stop();
            return false;
        }

        if (!renderer_init(&app.window, previous_backend, pcfg->vsync)) {
            RL_FATAL("Failed to restore previous renderer backend %d", previous_backend);
            rl_engine_stop();
            return false;
        }

        RL_WARN("Renderer rollback complete");
        return false;
    }

    pcfg->renderer_backend = backend;
    config_mark_dirty();
    gui_set_layout_dimensions((f32)app.window.settings.width, (f32)app.window.settings.height);

    RL_INFO("Renderer backend switched successfully to %d", backend);
    return true;
}

b8 create_editor(void) {
    rl_engine_config engine_config = rl_engine_config_default();
    engine_config.asset_root = "../../../assets/";

    app.focused = true;
    app.backend_switch_requested = false;
    app.requested_backend = BACKEND_OPENGL;

    if (!rl_engine_create(&engine_config)) {
        RL_FATAL("Engine failed to bootstrap");
        return false;
    }

    rl_config *cfg = config_get();
    platform_window_settings win_settings = config_to_window_settings(cfg, "Realm Editor");

    if (!ed_window_create(&app.window, &win_settings)) {
        return false;
    }

    config_track_window(&app.window);

    if (!renderer_init(&app.window, cfg->renderer_backend, cfg->vsync)) {
        if (cfg->renderer_backend != BACKEND_OPENGL) {
            RL_WARN("Renderer init failed (backend=%d). Falling back to OpenGL.", cfg->renderer_backend);
            cfg->renderer_backend = BACKEND_OPENGL;
            config_mark_dirty();
            if (!renderer_init(&app.window, cfg->renderer_backend, cfg->vsync)) {
                RL_ERROR("Failed to initialize renderer");
                return false;
            }
        } else {
            RL_ERROR("Failed to initialize renderer");
            return false;
        }
    }

    init_gui((f32)app.window.settings.width, (f32)app.window.settings.height);

    // Console registers events first so it can consume key/char input when visible
    ed_console_init(&app.console);
    ed_layout_init(&app.layout);
    ed_event_handler_init(&app.event_handler, &app);

    // Frame loop
    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        gui_layout_begin((f32)dt);
        ed_layout_render(&app.layout, &app, (f32)dt);
        gui_layout_end();

        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            ed_switch_backend(app.requested_backend);
        }
    }

    ed_console_shutdown(&app.console);
    rl_engine_destroy();

    return true;
}
