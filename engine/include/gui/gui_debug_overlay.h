#pragma once

#include "defines.h"

typedef struct gui_debug_overlay_cfg {
    b8 show_perf;     // FPS + frame time
    b8 show_renderer; // backend, vsync, resolution
    b8 show_memory;   // heap + committed
} gui_debug_overlay_cfg;

// Renders debug stats as a themed panel. Caller handles positioning.
REALM_API void gui_debug_overlay(const gui_debug_overlay_cfg *cfg);
