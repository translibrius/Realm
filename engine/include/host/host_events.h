#pragma once

#include "defines.h"
#include "host/host_console.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

// Callback invoked when F10 is pressed. The host sets its own
// backend_switch_requested / requested_backend flags.
typedef void (*host_backend_switch_fn)(void *userdata, RENDERER_BACKEND new_backend);

typedef struct host_event_ctx {
    platform_window *window;
    b8 *focused;
    host_console *console;
    host_backend_switch_fn on_backend_switch;
    void *userdata;
    b8 raw_input_on_borderless; // realm=true, editor=false
} host_event_ctx;

// Register shared event handlers (focus, resize, grave, F9, F10, F11, scroll, file drop).
REALM_API void host_events_init(host_event_ctx *ctx);
