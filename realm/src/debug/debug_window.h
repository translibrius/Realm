#pragma once

#include "defines.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

typedef struct debug_window_state {
    platform_window window;
    b8 open;
    b8 toggle_requested;
} debug_window_state;

b8 debug_window_open(debug_window_state *dbg, platform_window *main_window, RENDERER_BACKEND backend);
void debug_window_close(debug_window_state *dbg);
void debug_window_tick(debug_window_state *dbg);
