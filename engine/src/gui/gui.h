#pragma once

#include "defines.h"

void init_gui(f32 width, f32 height);
void gui_begin_frame(f32 dt);
void gui_end_frame(void);
void gui_set_layout_dimensions(f32 width, f32 height);
