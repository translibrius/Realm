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
} gui_scroll_state;

// Opens a scrollable container. Place children between begin/end.
REALM_API void gui_scroll_begin(const char *id, gui_scroll_state *state, const gui_scroll_cfg *cfg);

// Closes the scrollable container, draws scrollbar, manages auto-scroll.
REALM_API void gui_scroll_end(const char *id, gui_scroll_state *state, const gui_scroll_cfg *cfg);
