#include <core/logger.h>

#include "application.h"

int main() {
    if (!create_application()) {
        RL_FATAL("Application failed to initialize, exiting...");
        return -1;
    }

    return 0;
}