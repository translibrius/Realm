#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_window_state {
    b8 visible;
    b8 dragging;
    f32 pos_x, pos_y;
} gui_window_state;

typedef struct gui_window_cfg {
    const char *title;      // title bar text
    f32 width;              // fixed width (required)
    f32 height;             // fixed height (required)
    Clay_Color bg_color;
    Clay_Color header_color;
    Clay_Color border_color;
    f32 corner_radius;      // default: 6
    u16 font;               // font for title + close button
    u16 font_size;          // default: 13
    i32 z_index;            // floating z-index (default: 100)
} gui_window_cfg;

// Opens a floating, draggable window with title bar and close button.
// Returns false if window is not visible — caller should skip children.
REALM_API b8 gui_window_begin(const char *id, gui_window_state *state, const gui_window_cfg *cfg);
REALM_API void gui_window_end(void);
