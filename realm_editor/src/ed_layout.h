#pragma once

#include "defines.h"
#include "gui/gui_scroll.h"

typedef struct ed_application ed_application;

typedef struct ed_layout {
    f32 left_panel_width;
    f32 right_panel_width;
    f32 bottom_panel_height;
    f32 menu_bar_height;
    gui_scroll_state hierarchy_scroll;
    gui_scroll_state properties_scroll;
} ed_layout;

void ed_layout_init(ed_layout *layout);
void ed_layout_render(ed_layout *layout, ed_application *app, f32 dt);
