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

    // Main loop
    if (!engine_run()) {
        RL_FATAL("Engine did not shut down gracefully");
    }

    return 0;
}