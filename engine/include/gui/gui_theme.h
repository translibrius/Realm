#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_theme {
    // Backgrounds
    Clay_Color bg;          // primary panel/window background
    Clay_Color bg_secondary; // secondary/raised surface
    Clay_Color bg_input;    // input field background

    // Accent
    Clay_Color accent;      // active/selected elements (buttons, sliders, checkboxes)
    Clay_Color accent_hover; // hovered accent

    // Text
    Clay_Color text;        // primary text color
    Clay_Color text_dim;    // secondary/dim text (labels, descriptions)

    // Borders
    Clay_Color border;      // default border color

    // Controls
    Clay_Color control;     // unchecked/inactive control background
    Clay_Color control_hover; // hovered control background
    Clay_Color control_thumb; // slider thumb, checkbox mark, etc.

    // Misc
    Clay_Color separator;   // separator line color
    f32 corner_radius;      // default corner radius
    u16 font_size;          // default font size
} gui_theme;

// Set the active theme. NULL resets to the built-in dark theme.
REALM_API void gui_theme_set(const gui_theme *theme);

// Get the currently active theme.
REALM_API const gui_theme *gui_theme_get(void);
