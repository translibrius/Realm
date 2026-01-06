#pragma once

#include "defines.h"

typedef struct application_config {
    const char *title;
} application_config;

typedef struct application {
    application_config config;
    void (*application_init)();
} application;