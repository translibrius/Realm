#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_context_menu_state {
    b8  open;
    b8  _just_opened; // internal: skip close-on-click for the opening frame
    f32 pos_x, pos_y;
    u32 _id;
} gui_context_menu_state;

typedef struct gui_context_menu_cfg {
    const char **items;
    i32 item_count;
    f32 width;            // default: 180
    Clay_Color color;     // default: t->bg_secondary
    Clay_Color hover_color;
    Clay_Color text_color;
    u16 font;
    u16 font_size;
} gui_context_menu_cfg;

REALM_API void gui_context_menu_open(gui_context_menu_state *state);
REALM_API i32  gui_context_menu(gui_context_menu_state *state, const gui_context_menu_cfg *cfg);
