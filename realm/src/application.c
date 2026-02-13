
#include "application.h"
#include "core/event.h"
#include "core/logger.h"
#include "engine.h"
#include "event_handler.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"

static rl_application_config config = {
    .title = "Realm",
    .vsync = false,
    .backend = BACKEND_VULKAN};

static rl_application app;

b8 create_window(const platform_window_settings *settings_override);
b8 create_app_module();
void destroy_app_module();
static b8 switch_renderer_backend(RENDERER_BACKEND backend);
static RENDERER_BACKEND get_next_backend(RENDERER_BACKEND backend);

b8 on_window_resize(void *event, void *data);
b8 on_key_press(void *event, void *data);

b8 create_application() {
    rl_engine_config engine_config = rl_engine_config_default();
    engine_config.asset_root = "../../../assets/";
    engine_config.log_level = LOG_INFO;

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

    if (!create_window(nullptr)) {
        return false;
    }

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

    if (!create_app_module()) {
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
                if (app.app_module.set_focused) {
                    app.app_module.set_focused(app.game_state, app.focused);
                }
                if (app.app_module.set_paused) {
                    app.app_module.set_paused(app.game_state, app.paused);
                }
                realm_app_watcher_mark_clean(&app.app_watcher);
            }
        }

        if (!realm_app_module_is_loaded(&app.app_module)) {
            rl_engine_end_frame();
            continue;
        }

        app.app_module.update(app.game_state, &app.app_context, dt);
        app.app_module.render(app.game_state, &app.app_context);
        rl_engine_end_frame();

        if (app.backend_switch_requested) {
            app.backend_switch_requested = false;
            switch_renderer_backend(app.requested_backend);
        }
    }

    realm_app_watcher_stop(&app.app_watcher);
    destroy_app_module();
    rl_engine_destroy();

    return true;
}

// Private

b8 create_app_module() {
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

    app.game_state = nullptr;

    app.game_state = mem_alloc(app.game_state_size, MEM_APPLICATION);
    if (!app.game_state) {
        RL_ERROR("failed to allocate app state");
        realm_app_module_unload(&app.app_module);
        return false;
    }
    mem_zero(app.game_state, app.game_state_size);

    app.app_context = (realm_app_context){
        .window = &app.window,
        .vsync = app.config.vsync,
        .renderer_backend = app.config.backend,
    };

    app.app_module.init(app.game_state, &app.app_context);
    return true;
}

void destroy_app_module() {
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

b8 create_window(const platform_window_settings *settings_override) {
    // Window
    if (settings_override) {
        app.window.settings = *settings_override;
    } else {
        app.window.settings = (platform_window_settings){
            .title = "Realm",
            .x = 0,
            .y = 0,
            .width = 500,
            .height = 500,
            .start_center = true,
            .window_flags = WINDOW_FLAG_DEFAULT,
            .window_mode = WINDOW_MODE_WINDOWED};
    }

    if (!platform_create_window(&app.window)) {
        RL_ERROR("failed to create main window");
        return false;
    }

    return true;
}

static RENDERER_BACKEND get_next_backend(RENDERER_BACKEND backend) {
    return backend == BACKEND_VULKAN ? BACKEND_OPENGL : BACKEND_VULKAN;
}

static b8 switch_renderer_backend(RENDERER_BACKEND backend) {
    if (backend == app.config.backend) {
        return true;
    }

    const RENDERER_BACKEND previous_backend = app.config.backend;
    const platform_window_settings previous_window_settings = app.window.settings;

    RL_INFO("Switching renderer backend: %d -> %d", previous_backend, backend);

    renderer_destroy();
    platform_destroy_window(app.window.id);

    if (!create_window(&previous_window_settings)) {
        RL_ERROR("Failed to recreate window for renderer switch");
        rl_engine_stop();
        return false;
    }

    if (!renderer_init(&app.window, backend, app.config.vsync)) {
        RL_ERROR("Failed to initialize renderer backend %d, attempting rollback", backend);

        renderer_destroy();
        platform_destroy_window(app.window.id);

        if (!create_window(&previous_window_settings)) {
            RL_FATAL("Failed to recreate window while rolling back renderer backend");
            rl_engine_stop();
            return false;
        }

        if (!renderer_init(&app.window, previous_backend, app.config.vsync)) {
            RL_FATAL("Failed to restore previous renderer backend %d", previous_backend);
            rl_engine_stop();
            return false;
        }

        app.config.backend = previous_backend;
        app.app_context.renderer_backend = previous_backend;
        app.app_context.window = &app.window;
        app.requested_backend = get_next_backend(previous_backend);
        return false;
    }

    app.config.backend = backend;
    app.app_context.renderer_backend = backend;
    app.app_context.window = &app.window;
    app.reload_requested = true;

    RL_INFO("Renderer backend switched successfully to %d. App module reload scheduled.", backend);
    return true;
}
