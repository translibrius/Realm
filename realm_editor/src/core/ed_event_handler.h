#pragma once

#include "defines.h"
#include "host/host_events.h"

typedef struct ed_application ed_application;

typedef struct ed_event_handler {
    ed_application *application;
    host_event_ctx host;
} ed_event_handler;

void ed_event_handler_init(ed_event_handler *handler, ed_application *application);
