
#include "application.h"
#include "core/event.h"
#include "core/logger.h"
#include "engine.h"
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
static b8 reload_requested = false;

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

    if (!renderer_init(&app.window, app.config.backend, app.config.vsync)) {
        RL_ERROR("failed to initialize renderer");
        return false;
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

        if (reload_requested) {
            reload_requested = false;
            RL_INFO("Reloading app module...");
            if (!realm_app_module_reload(&app.app_module, app.app_state, &app.app_context)) {
                RL_ERROR("App module reload failed");
            }
        }

        if (!realm_app_module_is_loaded(&app.app_module)) {
            rl_engine_end_frame();
            continue;
        }

        app.app_module.update(app.app_state, &app.app_context, dt);
        app.app_module.render(app.app_state, &app.app_context);
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

    app.app_state_size = app.app_module.get_state_size();
    app.app_state = nullptr;

    if (app.app_state_size > 0) {
        app.app_state = mem_alloc(app.app_state_size, MEM_APPLICATION);
        if (!app.app_state) {
            RL_ERROR("failed to allocate app state");
            return false;
        }
        mem_zero(app.app_state, app.app_state_size);
    }

    app.app_context = (realm_app_context){
        .vsync = app.config.vsync,
        .renderer_backend = app.config.backend,
        .width = app.window.settings.width,
        .height = app.window.settings.height,
        .x = app.window.settings.x,
        .y = app.window.settings.y,
    };

    app.app_module.init(app.app_state, &app.app_context);
    return true;
}

void destroy_app_module() {
    if (realm_app_module_is_loaded(&app.app_module)) {
        app.app_module.shutdown(app.app_state, &app.app_context);
    }
    if (app.app_state) {
        mem_free(app.app_state, app.app_state_size, MEM_APPLICATION);
    }
    app.app_state = nullptr;
    app.app_state_size = 0;
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

    event_register(EVENT_WINDOW_RESIZE, on_window_resize, nullptr);
    event_register(EVENT_KEY_PRESS, on_key_press, nullptr);

    return true;
}

b8 on_window_resize(void *event, void *data) {
    platform_window *window = event;
    platform_window *renderer_window = renderer_get_active_window();

    if (window->id == app.window.id) {
        app.window.settings = window->settings;
        app.app_context.width = window->settings.width;
        app.app_context.height = window->settings.height;
        app.app_context.x = window->settings.x;
        app.app_context.y = window->settings.y;
        if (renderer_window->id == window->id) {
            renderer_resize_framebuffer(window->settings.width, window->settings.height);
        }
    }
    // Consume event
    return true;
}

b8 on_key_press(void *event, void *data) {
    (void)data;
    input_key *key = event;

    if (!key) {
        return false;
    }

    if (key->key == KEY_F5 && key->pressed) {
        reload_requested = true;
    }

    return false;
}
