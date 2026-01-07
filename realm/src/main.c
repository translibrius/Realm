#include "profiler/profiler.h"
#include <core/logger.h>
#include <engine.h>

#include "application.h"

int main() {
    TracyCSetThreadName("Main");
    if (!create_engine()) {
        RL_FATAL("Engine failed to bootstrap");
    }

    application app;
    if (!create_application(&app)) {
        RL_FATAL("Application failed to initialize, exiting...");
        return -1;
    }

    f64 dt = 0.0f;
    while (engine_is_running()) {
        TracyCFrameMark
        if (!engine_begin_frame(&dt)) {
            continue;
        }

        game_update(&app.game_inst, dt);
        game_render(&app.game_inst, dt);
        engine_end_frame();
    }

    destroy_engine();
    return 0;
}