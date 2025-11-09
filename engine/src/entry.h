#pragma once

#include "defines.h"
#include "engine.h"
#include "core/logger.h"
#include "util/assert.h"

#include <stdio.h>

// This should be defined by the application using engine.dll
extern b8 create_application(application* application);

int main() {
    application app;
    if (!create_application(&app)) {
        RL_ERROR("Failed to create application");
        return -1;
    }

    if (!create_engine(&app)) {
        RL_ERROR("Engine failed to create");
        return -1;
    }

    // Main loop
    if (!engine_run()) {
        RL_ERROR("Engine did not shut down gracefully");
        return 2;
    }
    
    return 0;
}
