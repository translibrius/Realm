#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_checkbox_cfg {
    Clay_Color color;         // box background when unchecked (default: dark gray)
    Clay_Color hover_color;   // box background when hovered (default: lighter gray)
    Clay_Color checked_color; // box background when checked (default: accent blue)
    Clay_Color check_color;   // inner checkmark color (default: white)
    f32 size;                 // box size in px (default: 18)
    f32 corner_radius;        // default: 3
} gui_checkbox_cfg;

// Single-call checkbox. Toggles *checked on click.
// Returns true if toggled this frame.
REALM_API b8 gui_checkbox(b8 *checked, const gui_checkbox_cfg *cfg);
