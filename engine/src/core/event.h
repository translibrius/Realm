#pragma once

#include "defines.h"
#include "util/math_types.h"

typedef struct e_resize_payload {
    u16 window_id;
    i32 x;
    i32 y;
    i32 width;
    i32 height;
} e_resize_payload;

typedef enum EVENT_TYPE {
    EVENT_WINDOW_RESIZE,

    // Splash
    EVENT_SPLASH_INCREMENT,
    EVENT_SPLASH_FINISHED,
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