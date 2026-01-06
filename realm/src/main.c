#include <core/camera.h>
#include <core/logger.h>
#include <engine.h>

#include "application.h"

int main() {
    if (!create_engine()) {
        RL_FATAL("Engine failed to create");
    }
    RL_INFO("--------------ENGINE_START--------------");

    application *app = create_application();

    while (!platform_window_should_close(app->main_window.id)) {
        f64 dt = engine_begin_frame();
        game_update(&app->game_inst, dt);
        game_render(&app->game_inst, dt);
        engine_end_frame();
    }

    destroy_engine();

    return 0;
}