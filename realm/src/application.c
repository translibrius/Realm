
#include "application.h"
#include "core/event.h"
#include "engine.h"
#include "game.h"
#include "platform/platform.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"

static rl_application_config config = {
    .title = "Realm",
    .vsync = false,
    .backend = BACKEND_VULKAN};
static rl_application app;

b8 create_game();
b8 create_window();

b8 on_window_resize(void *event, void *data);

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

    if (!create_game()) {
        return false;
    }

    f64 dt = 0.0f;
    while (rl_engine_is_running()) {
        TracyCFrameMark if (!rl_engine_begin_frame(&dt)) {
            continue;
        }

        game_update(&app.game, dt);
        game_render(&app.game, dt);
        rl_engine_end_frame();
    }

    rl_engine_destroy();

    return true;
}

// Private

b8 create_game() {
    rl_game_cfg game_cfg = {
        .vsync = app.config.vsync,
        .renderer_backend = app.config.backend,
        .width = app.window.settings.width,
        .height = app.window.settings.height,
        .x = 0,
        .y = 0};

    if (!game_init(&app.game, game_cfg)) {
        RL_ERROR("failed to initialize game instance");
        return false;
    }

    return true;
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

    return true;
}

b8 on_window_resize(void *event, void *data) {
    platform_window *window = event;
    platform_window *renderer_window = renderer_get_active_window();

    if (window->id == app.window.id) {
        app.window.settings = window->settings;
        if (renderer_window->id == window->id) {
            renderer_resize_framebuffer(window->settings.width, window->settings.height);
        }
    }
    // Consume event
    return true;
}
