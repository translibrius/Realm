#pragma once

#include "defines.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_scroll.h"
#include "gui/gui_splitter.h"
#include "gui/gui_tree.h"

typedef struct ed_application ed_application;

typedef struct ed_layout {
    f32 left_panel_width;
    f32 right_panel_width;
    f32 bottom_panel_height;
    f32 menu_bar_height;
    gui_scroll_state hierarchy_scroll;
    gui_scroll_state properties_scroll;

    // Splitters
    gui_splitter_state splitter_left;
    gui_splitter_state splitter_right;
    gui_splitter_state splitter_bottom;

    // Hierarchy tree
    gui_tree_state hierarchy_tree;
    b8 dummy_root_expanded;
    b8 dummy_objects_expanded;

    // Menu bar dropdowns
    gui_dropdown_state menu_file;
    gui_dropdown_state menu_edit;
    gui_dropdown_state menu_view;
} ed_layout;

void ed_layout_init(ed_layout *layout);
void ed_layout_render(ed_layout *layout, ed_application *app, f32 dt);
