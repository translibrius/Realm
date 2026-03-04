#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_window_state {
    b8 visible;
    b8 dragging;
    f32 pos_x, pos_y;
    u32 _id; // 0 = uninitialized, auto-generated on first gui_window_begin
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

typedef struct gui_window_result {
    b8 visible;       // false if window was already hidden — caller should skip children
    b8 close_clicked; // close button was clicked this frame — caller decides what to do
} gui_window_result;

// Opens a floating, draggable window with title bar and close button.
// Check result.visible to decide whether to render children.
// Check result.close_clicked to handle close (e.g. set state->visible = false).
REALM_API gui_window_result gui_window_begin(gui_window_state *state, const gui_window_cfg *cfg);
REALM_API void gui_window_end(void);
