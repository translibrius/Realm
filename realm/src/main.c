#include <stdio.h>
#include <string.h>

#include <core/logger.h>

#include "application.h"

int main(int argc, char **argv) {
    const char *project_path = nullptr;
    for (i32 i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--project") == 0) {
            project_path = argv[i + 1];
            break;
        }
    }

    if (!project_path) {
        printf("Usage: Realm --project <path>\n");
        return 0;
    }

    if (!create_application(project_path)) {
        RL_FATAL("Application failed to initialize, exiting...");
        return -1;
    }

    return 0;
}