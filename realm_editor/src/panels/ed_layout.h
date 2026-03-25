#pragma once

#include "defines.h"
#include "panels/ed_inspector.h"
#include "gui/gui_context_menu.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_number_input.h"
#include "gui/gui_scroll.h"
#include "gui/gui_slider.h"
#include "gui/gui_splitter.h"
#include "gui/gui_tree.h"

#include "clay.h"

#define ED_ENTITY_NODE_BASE 0x10000u

typedef struct ed_application ed_application;
typedef struct ed_undo_stack ed_undo_stack;

typedef struct ed_layout {
    f32 left_panel_width;
    f32 right_panel_width;
    f32 bottom_panel_height;
    f32 menu_bar_height;
    f32 left_split_height; // height of hierarchy portion in left panel
    gui_scroll_state hierarchy_scroll;
    gui_scroll_state properties_scroll;

    // Splitters
    gui_splitter_state splitter_left;
    gui_splitter_state splitter_right;
    gui_splitter_state splitter_bottom;
    gui_splitter_state splitter_left_h; // horizontal split within left panel

    // Hierarchy tree
    gui_tree_state hierarchy_tree;
    b8 scene_root_expanded;

    // Menu bar dropdowns
    gui_dropdown_state menu_file;
    gui_dropdown_state menu_edit;
    gui_dropdown_state menu_view;

    // Viewport tab bar (0 = Viewport, 1 = Settings)
    i32 viewport_tab;
    gui_dropdown_state theme_dropdown;

    // Settings subtabs + widget state
    i32 settings_tab;
    gui_scroll_state settings_scroll;
    gui_number_input_state settings_cam_speed;
    gui_number_input_state settings_cam_sens;
    gui_number_input_state settings_cam_fov;
    gui_slider_state settings_slider_speed;
    gui_slider_state settings_slider_sens;
    gui_slider_state settings_slider_fov;

    // Viewport bounds from previous frame (for camera input)
    Clay_BoundingBox viewport_bounds;

    // Property inspector
    ed_inspector inspector;

    // Context menus
    gui_context_menu_state hierarchy_ctx_menu;
    gui_context_menu_state viewport_ctx_menu;
} ed_layout;

void ed_layout_init(ed_layout *layout, ed_undo_stack *undo);
void ed_layout_render(ed_layout *layout, ed_application *app, f32 dt);
