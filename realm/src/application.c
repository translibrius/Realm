#include "application.h"

#include "engine.h"
#include "gui/gui.h"
#include "platform/platform.h"

static application_config config = {
    .title = "Realm"
};

b8 create_application(application *app) {
    if (!app) {
        RL_ERROR("create_application(): failed to allocate application");
        return false;
    }

    app->config = config;

    platform_window *window = &app->main_window;
    window->settings = (platform_window_settings){
        .title = app->config.title,
        .x = 0,
        .y = 0,
        .width = 500,
        .height = 500,
        .start_center = true,
        .window_flags = WINDOW_FLAG_DEFAULT,
        .window_mode = WINDOW_MODE_WINDOWED
    };

    if (!platform_create_window(&app->main_window)) {
        RL_ERROR("create_application(): failed to create main window");
        return false;
    }

    platform_set_raw_input(&app->main_window, true);
    platform_set_cursor_mode(&app->main_window, CURSOR_MODE_LOCKED);

    init_gui((f32)app->main_window.settings.width, (f32)app->main_window.settings.height);

    camera_init(&app->camera);
    if (!engine_renderer_init(&app->main_window, &app->camera)) {
        RL_ERROR("create_application(): failed to initialize renderer");
        return false;
    }

    if (!game_init(&app->game_inst, &app->main_window)) {
        RL_ERROR("create_application(): failed to initialize game instance");
        return false;
    }

    return true;
}