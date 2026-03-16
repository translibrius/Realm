#pragma once

#include "clay.h"
#include "defines.h"

typedef enum gui_icon_type {
    GUI_ICON_TRIANGLE_RIGHT = 1,
    GUI_ICON_TRIANGLE_DOWN  = 2,
} gui_icon_type;

REALM_API void gui_icon(gui_icon_type type, f32 size, Clay_Color color);
