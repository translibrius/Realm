#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_dropdown_state {
    i32 selected;  // currently selected index (-1 = none)
    b8 open;       // dropdown list is visible
    u32 _id;       // 0 = auto-generated on first use
} gui_dropdown_state;

typedef struct gui_dropdown_cfg {
    const char **items; // array of null-terminated strings
    i32 item_count;
    f32 width;          // dropdown width (default: 200)
    Clay_Color color;        // closed dropdown background
    Clay_Color hover_color;  // hovered item background
    Clay_Color text_color;   // text color
    f32 corner_radius;
    u16 font;
    u16 font_size;      // default: 14
} gui_dropdown_cfg;

// Single-call dropdown. Updates state->selected on selection.
// Returns true if the selection changed this frame.
REALM_API b8 gui_dropdown(gui_dropdown_state *state, const gui_dropdown_cfg *cfg);
