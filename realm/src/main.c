#include "profiler/profiler.h"
#include <core/logger.h>

#include "app/application.h"

int main() {
    TracyCSetThreadName("Main");

    if (!create_application()) {
        RL_FATAL("Application failed to initialize, exiting...");
        return -1;
    }

    return 0;
}