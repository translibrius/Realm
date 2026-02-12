
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

b8 create_window();
b8 create_app_module();
void destroy_app_module();

b8 on_window_resize(void *event, void *data);
b8 on_key_press(void *event, void *data);

b8 create_application() {
    app.config = config;

    if (!rl_engine_create()) {
        RL_FATAL("Engine failed to bootstrap");
    }

    if (!create_window()) {
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

    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        RL_PROFILE_FRAME_MARK();
        if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        if (app.reload_requested) {
            app.reload_requested = false;
            RL_INFO("Reloading app module...");
            if (!realm_app_module_reload(&app.app_module, app.game_state, &app.app_context)) {
                RL_ERROR("App module reload failed");
            }
        }

        if (!realm_app_module_is_loaded(&app.app_module)) {
            rl_engine_end_frame();
            continue;
        }

        app.app_module.update(app.game_state, &app.app_context, dt);
        app.app_module.render(app.game_state, &app.app_context);
        rl_engine_end_frame();
    }

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

    u64 game_state_size = sizeof(rl_game);
    app.game_state = nullptr;

    app.game_state = mem_alloc(game_state_size, MEM_APPLICATION);
    if (!app.game_state) {
        RL_ERROR("failed to allocate app state");
        return false;
    }
    mem_zero(app.game_state, game_state_size);

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
        mem_free(app.game_state, sizeof(rl_game), MEM_APPLICATION);
    }
    app.game_state = nullptr;
    realm_app_module_unload(&app.app_module);
}

b8 create_window() {
    // Window
    app.window.settings = (platform_window_settings){
        .title = "Realm",
        .x = 0,
        .y = 0,
        .width = 500,
        .height = 500,
        .start_center = true,
        .window_flags = WINDOW_FLAG_DEFAULT,
        .window_mode = WINDOW_MODE_WINDOWED};

    if (!platform_create_window(&app.window)) {
        RL_ERROR("failed to create main window");
        return false;
    }

    return true;
}
