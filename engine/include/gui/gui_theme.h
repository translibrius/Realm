#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_theme {
    // Backgrounds
    Clay_Color bg;           // primary panel/window background
    Clay_Color bg_secondary; // secondary/raised surface (headers, active tabs)
    Clay_Color bg_input;     // input field background
    Clay_Color bg_overlay;   // dim overlay backdrop (pause menu)

    // Accent
    Clay_Color accent;       // active/selected elements (sliders, checkboxes)
    Clay_Color accent_hover; // hovered accent

    // Text
    Clay_Color text;         // primary text color
    Clay_Color text_dim;     // secondary/dim text (labels, descriptions)

    // Borders
    Clay_Color border;       // default border color

    // Controls
    Clay_Color control;       // inactive control background (buttons, unchecked)
    Clay_Color control_hover; // hovered control background
    Clay_Color control_press; // pressed control background
    Clay_Color control_thumb; // slider thumb, checkbox mark, etc.

    // Danger
    Clay_Color danger;       // destructive actions, close button hover
    Clay_Color danger_press; // pressed danger

    // Log-level colors (console)
    Clay_Color log_info;
    Clay_Color log_warn;
    Clay_Color log_error;
    Clay_Color log_fatal;
    Clay_Color log_debug;
    Clay_Color log_trace;

    // Debug overlay
    Clay_Color debug_highlight; // bright value text (FPS, frame time)

    // Misc
    Clay_Color separator;    // separator line color
    Clay_Color viewport_bg;  // 3D viewport clear color (0-255, converted to float for renderer)
    f32 corner_radius;       // default corner radius
    u16 font_size;           // default font size
} gui_theme;

// Set the active theme. NULL resets to the built-in dark theme.
REALM_API void gui_theme_set(const gui_theme *theme);

// Get the currently active theme.
REALM_API const gui_theme *gui_theme_get(void);

// Built-in theme presets.
REALM_API const gui_theme *gui_theme_dark(void);
REALM_API const gui_theme *gui_theme_catppuccin(void);
REALM_API const gui_theme *gui_theme_dracula(void);
REALM_API const gui_theme *gui_theme_gruvbox(void);
REALM_API const gui_theme *gui_theme_nord(void);
REALM_API const gui_theme *gui_theme_tokyonight(void);
REALM_API const gui_theme *gui_theme_onedark(void);
REALM_API const gui_theme *gui_theme_rosepine(void);
