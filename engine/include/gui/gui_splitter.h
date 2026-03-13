#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_splitter_state {
    b8  dragging;
    u32 _id;
} gui_splitter_state;

typedef struct gui_splitter_cfg {
    f32        thickness;   // default: 5
    f32        min_value;   // default: 50
    f32        max_value;   // default: 800
    Clay_Color color;       // default: theme separator
    Clay_Color hover_color; // default: theme accent
    b8         invert;      // negate delta (for right/bottom panels)
} gui_splitter_cfg;

// Vertical splitter (dragged left/right). Returns true if *target changed.
REALM_API b8 gui_splitter_v(gui_splitter_state *state, f32 *target, const gui_splitter_cfg *cfg);

// Horizontal splitter (dragged up/down). Returns true if *target changed.
REALM_API b8 gui_splitter_h(gui_splitter_state *state, f32 *target, const gui_splitter_cfg *cfg);
