#pragma once

#include "clay.h"
#include "defines.h"
#include "gui/gui_text.h"

typedef struct gui_field_cfg {
    f32 label_width; // fixed label column width (default: 120)
    f32 gap;         // gap between label and control (default: 8)
    u16 font;        // label font (default: 0)
    u16 font_size;   // label font size (default: 14)
    Clay_Color label_color; // label text color (default: light gray)
} gui_field_cfg;

// Opens a horizontal row with a label on the left and a control area on the right.
// Place your widget (slider, checkbox, text, etc.) between begin/end.
REALM_API void gui_field_begin(const char *label, const gui_field_cfg *cfg);
REALM_API void gui_field_end(void);
