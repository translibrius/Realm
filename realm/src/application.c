#include "application.h"

#include "engine.h"
#include "gui/gui.h"
#include "platform/platform.h"

static application_config config = {
    .title = "Realm"
};
static application state;

application *create_application() {
    state.config = config;

    platform_window *window = &state.main_window;
    window->settings = (platform_window_settings){
        .title = state.config.title,
        .x = 0,
        .y = 0,
        .width = 500,
        .height = 500,
        .start_center = true,
        .stop_on_close = true,
        .window_flags = WINDOW_FLAG_DEFAULT,
        .window_mode = WINDOW_MODE_WINDOWED
    };

    platform_create_window(&state.main_window);

    platform_set_raw_input(&state.main_window, true);
    platform_set_cursor_mode(&state.main_window, CURSOR_MODE_LOCKED);

    init_gui((f32)state.main_window.settings.width, (f32)state.main_window.settings.height);

    camera_init(&state.camera);
    engine_renderer_init(&state.main_window, &state.camera);

    game_init(&state.game_inst, (f32)state.main_window.settings.width, (f32)state.main_window.settings.height);

    return &state;
}