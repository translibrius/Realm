#pragma once

#include "defines.h"
#include "util/math_types.h"

typedef struct e_resize_payload {
    vec2 position;
    vec2 size;
} e_resize_payload;

typedef enum EVENT_TYPE {
    EVENT_WINDOW_RESIZE,
} EVENT_TYPE;

typedef struct rl_event {
    EVENT_TYPE type;
    b8 (*event_callback)(void *data);
} rl_event;

u64 event_system_size();
b8 event_system_start(void *memory);

void event_fire(EVENT_TYPE type, void *data);
void event_register(EVENT_TYPE type, b8 (*callback)(void *data));

void event_system_shutdown();