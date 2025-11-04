#pragma once

#include "defines.h"

#include "platform/platform.h"
#include <stdio.h>

#include "util/assert.h"

typedef struct application {
    const char* title;
} application;

// This should be defined by the application using engine.dll
extern b8 create_application(application* application);

int main() {
    application app;
    if (!create_application(&app)) {
        printf("Failed to create application");
        return -1;
    }
    platform_create_window(app.title);

    return 0;
}