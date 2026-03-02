#pragma once

#include "defines.h"

REALM_API void init_gui(f32 width, f32 height);
REALM_API void gui_begin_frame(f32 dt);
REALM_API void gui_end_frame(void);
REALM_API void gui_set_layout_dimensions(f32 width, f32 height);
