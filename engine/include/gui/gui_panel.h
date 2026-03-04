#pragma once

#include "clay.h"
#include "defines.h"

typedef enum gui_sizing {
    GUI_SIZE_FIT   = 0, // default: shrink-to-fit content
    GUI_SIZE_GROW  = 1, // expand to fill parent
    GUI_SIZE_FIXED = 2, // use the explicit width/height value
} gui_sizing;

typedef struct gui_panel_cfg {
    Clay_Color color;        // background color (default: transparent)
    Clay_Color border_color; // border color
    f32 border_width;        // uniform border width (0 = no border)
    f32 corner_radius;       // uniform corner radius
    f32 padding;             // uniform padding
    f32 gap;                 // child gap
    f32 width;               // value for GUI_SIZE_FIXED (also auto-promotes if > 0 and sizing is FIT)
    f32 height;              // value for GUI_SIZE_FIXED (also auto-promotes if > 0 and sizing is FIT)
    gui_sizing width_sizing;  // default: GUI_SIZE_FIT
    gui_sizing height_sizing; // default: GUI_SIZE_FIT
    b8 horizontal;           // false = top-to-bottom, true = left-to-right
} gui_panel_cfg;

REALM_API void gui_panel_begin(const gui_panel_cfg *cfg);
REALM_API void gui_panel_end(void);

// Flexible spacer — grows to fill available space.
REALM_API void gui_spacer(void);

// Fixed-size spacer.
REALM_API void gui_spacer_fixed(f32 size);
