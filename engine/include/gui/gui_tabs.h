#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_tabs_cfg {
    Clay_Color color;        // inactive tab background
    Clay_Color active_color; // active tab background
    Clay_Color hover_color;  // hovered tab background
    Clay_Color text_color;   // tab text color
    f32 padding;             // tab padding (default: 8)
    f32 gap;                 // gap between tabs (default: 2)
    f32 corner_radius;       // default: 4
    u16 font;
    u16 font_size;           // default: 14
} gui_tabs_cfg;

// Renders a horizontal tab bar. Returns the new selected index.
// *selected is updated in place on tab click.
REALM_API i32 gui_tabs(i32 *selected, const char **labels, i32 count, const gui_tabs_cfg *cfg);
