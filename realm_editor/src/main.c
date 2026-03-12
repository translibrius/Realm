#include <core/logger.h>

#include "ed_application.h"

int main() {
    if (!create_editor()) {
        RL_FATAL("Editor failed to initialize, exiting...");
        return -1;
    }

    return 0;
}
