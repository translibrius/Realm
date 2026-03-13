#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_scroll_cfg {
    f32 scrollbar_width; // 0 = no scrollbar (default: 8)
    Clay_Color track_color;
    Clay_Color thumb_color;
    f32 thumb_radius;
} gui_scroll_cfg;

typedef struct gui_scroll_state {
    b8 auto_scroll;
    u32 _id;              // 0 = uninitialized, auto-generated on first gui_scroll_begin
    Clay_Vector2 _offset; // cached scroll offset — avoids Clay_GetScrollOffset pointer bug
} gui_scroll_state;

// Opens a scrollable container. Place children between begin/end.
REALM_API void gui_scroll_begin(gui_scroll_state *state, const gui_scroll_cfg *cfg);

// Closes the scrollable container, draws scrollbar, manages auto-scroll.
REALM_API void gui_scroll_end(void);
