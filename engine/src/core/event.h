#pragma once

#include "defines.h"

typedef struct e_resize_payload {
    u16 window_id;
    i32 x;
    i32 y;
    i32 width;
    i32 height;
} e_resize_payload;

typedef enum EVENT_TYPE {
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST,

    // Input
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_CLICK,
    EVENT_MOUSE_SCROLL,
    EVENT_KEY_PRESS,

    // Splash
    EVENT_SPLASH_INCREMENT,
} EVENT_TYPE;

typedef struct rl_event {
    EVENT_TYPE type;
    b8 (*event_callback)(void *event, void *user_data);
    void *user_data;
} rl_event;

u64 event_system_size();
b8 event_system_start(void *memory);

void event_fire(EVENT_TYPE type, void *event_data);
void event_register(EVENT_TYPE type, b8 (*callback)(void *data, void *user_data), void *user_data);

void event_system_shutdown();