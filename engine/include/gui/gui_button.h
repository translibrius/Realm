#pragma once

#include "clay.h"
#include "defines.h"
#include "gui/gui_text.h"

typedef struct gui_button_state {
    b8 hovered;
    b8 pressed;  // mouse is down on this element
    b8 clicked;  // mouse released on this element (the action trigger)
} gui_button_state;

typedef struct gui_button_cfg {
    Clay_Color color;       // normal background
    Clay_Color hover_color; // hovered background
    Clay_Color press_color; // pressed background
    f32 padding;
    f32 corner_radius;          // uniform corner radius (shorthand)
    Clay_CornerRadius corners;   // per-corner radius (overrides corner_radius if any > 0)
    f32 width;  // 0 = fit
    f32 height; // 0 = fit
    b8 grow_width;  // true = expand to fill parent width
    b8 align_left;  // true = left-align children instead of center
    b8 no_bg;       // true = transparent background in normal state
    f32 gap;        // child gap (for icon+text buttons)
} gui_button_cfg;

// Opens a button element. Returns interaction state for this frame.
// Place children (text, icons) between begin/end.
REALM_API gui_button_state gui_button_begin(const gui_button_cfg *cfg);
REALM_API void gui_button_end(void);

// Single-call text button. Both cfg pointers are optional (NULL = defaults).
REALM_API gui_button_state gui_text_button(const char *label, const gui_button_cfg *btn_cfg, const gui_text_cfg *text_cfg);
