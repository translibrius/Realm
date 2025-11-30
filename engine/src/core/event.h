#pragma once

#include "defines.h"

typedef enum EVENT_TYPE {
    WINDOW_RESIZE,
} EVENT_TYPE;

typedef struct rl_event {
    u32 id;
    EVENT_TYPE type;
    b8 (*event_callback)();
} rl_event;

typedef struct event_system_state {
    rl_event *events;
} event_system_state;

static event_system_state state;