#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_panel_cfg {
    Clay_Color color;        // background color (default: transparent)
    Clay_Color border_color; // border color
    f32 border_width;        // uniform border width (0 = no border)
    f32 corner_radius;       // uniform corner radius
    f32 padding;             // uniform padding
    f32 gap;                 // child gap
    f32 width;               // 0 = fit content
    f32 height;              // 0 = fit content
    b8 grow_width;           // GROW instead of FIT/FIXED for width
    b8 grow_height;          // GROW instead of FIT/FIXED for height
    b8 horizontal;           // false = top-to-bottom, true = left-to-right
} gui_panel_cfg;

REALM_API void gui_panel_begin(const char *id, const gui_panel_cfg *cfg);
REALM_API void gui_panel_end(void);

// Flexible spacer — grows to fill available space.
REALM_API void gui_spacer(void);

// Fixed-size spacer.
REALM_API void gui_spacer_fixed(f32 size);
