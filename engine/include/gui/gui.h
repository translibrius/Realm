#pragma once

#include "defines.h"

REALM_API void init_gui(f32 width, f32 height);
REALM_API void gui_begin_frame(f32 dt);
REALM_API void gui_end_frame(void);
REALM_API void gui_set_layout_dimensions(f32 width, f32 height);

REALM_API void gui_layout_begin(f32 dt);
REALM_API void gui_layout_end(void);

// Measure the pixel width of text[0..len-1] using the given font and size.
REALM_API f32 gui_measure_text_width(const char *text, u16 len, u16 font_id, u16 font_size);
