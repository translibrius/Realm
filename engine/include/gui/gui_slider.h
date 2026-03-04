#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_slider_state {
    f32 value;    // 0.0 to 1.0
    b8 dragging;
    u32 _id;      // 0 = auto-generated on first use
} gui_slider_state;

typedef struct gui_slider_cfg {
    f32 width;              // track width (default: 200)
    f32 height;             // track height (default: 16)
    Clay_Color track_color; // background (default: dark gray)
    Clay_Color fill_color;  // filled portion (default: accent blue)
    Clay_Color thumb_color; // thumb (default: white)
    f32 corner_radius;      // track + fill radius (default: height/2)
} gui_slider_cfg;

// Single-call slider. Updates state->value on interaction.
// Returns true if the value changed this frame.
REALM_API b8 gui_slider(gui_slider_state *state, const gui_slider_cfg *cfg);
