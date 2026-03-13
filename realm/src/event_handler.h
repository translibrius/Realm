#pragma once

#include "defines.h"
#include "host/host_events.h"

typedef struct rl_application rl_application;

typedef struct app_event_handler {
    rl_application *application;
    host_event_ctx host;
} app_event_handler;

void app_event_handler_init(app_event_handler *handler, rl_application *application);
