#pragma once

#include "clay.h"
#include "defines.h"
#include "gui/gui_icon.h"

typedef struct gui_tree_state {
    u32 selected_id; // 0 = none
    u32 _id;
} gui_tree_state;

typedef struct gui_tree_cfg {
    f32        indent;       // default: 16
    f32        row_height;   // default: 22
    Clay_Color select_color; // default: theme accent
    Clay_Color hover_color;  // default: theme control_hover
    Clay_Color text_color;   // default: theme text
    Clay_Color arrow_color;  // default: theme text_dim
    u16        font;
    u16        font_size;    // default: 13
} gui_tree_cfg;

typedef struct gui_tree_node_result {
    b8 selected; // became selected this frame
    b8 expanded; // current expand state
} gui_tree_node_result;

REALM_API void gui_tree_begin(gui_tree_state *state, const gui_tree_cfg *cfg);
REALM_API void gui_tree_end(void);
REALM_API gui_tree_node_result gui_tree_node_begin(u32 node_id, const char *label, b8 *expanded, b8 leaf, gui_icon_type icon);
REALM_API void gui_tree_node_end(void);
