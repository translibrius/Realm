#include "gui/gui_theme.h"

static const gui_theme dark_theme = {
    .bg            = {25, 25, 28, 230},
    .bg_secondary  = {38, 38, 44, 255},
    .bg_input      = {18, 18, 22, 255},
    .bg_overlay    = {10, 10, 12, 180},

    .accent        = {55, 110, 190, 200},
    .accent_hover  = {70, 130, 210, 255},

    .text          = {235, 235, 240, 255},
    .text_dim      = {155, 155, 165, 255},

    .border        = {65, 65, 72, 255},

    .control       = {52, 52, 60, 255},
    .control_hover = {68, 68, 78, 255},
    .control_press = {42, 42, 48, 255},
    .control_thumb = {230, 230, 235, 255},

    .danger        = {180, 60, 60, 255},
    .danger_press  = {200, 40, 40, 255},

    .log_info      = {128, 230, 255, 255},
    .log_warn      = {255, 191, 51, 255},
    .log_error     = {255, 89, 89, 255},
    .log_fatal     = {255, 50, 50, 255},
    .log_debug     = {160, 160, 160, 255},
    .log_trace     = {120, 120, 120, 255},

    .debug_highlight = {180, 255, 180, 255},

    .separator     = {55, 55, 62, 255},
    .corner_radius = 4,
    .font_size     = 14,
};

// Catppuccin Mocha — https://catppuccin.com/palette
static const gui_theme catppuccin_theme = {
    .bg            = {30, 30, 46, 230},     // base
    .bg_secondary  = {49, 50, 68, 255},     // surface0
    .bg_input      = {24, 24, 37, 255},     // mantle
    .bg_overlay    = {17, 17, 27, 180},     // crust with alpha

    .accent        = {116, 160, 230, 200},  // blue (softened for selection)
    .accent_hover  = {137, 180, 250, 255},  // blue (full)

    .text          = {205, 214, 244, 255},  // text
    .text_dim      = {166, 173, 200, 255},  // subtext0

    .border        = {69, 71, 90, 255},     // surface1

    .control       = {49, 50, 68, 255},     // surface0
    .control_hover = {69, 71, 90, 255},     // surface1
    .control_press = {39, 39, 55, 255},     // between base and mantle
    .control_thumb = {186, 194, 222, 255},  // subtext1

    .danger        = {243, 139, 168, 255},  // red
    .danger_press  = {235, 160, 172, 255},  // flamingo

    .log_info      = {137, 220, 235, 255},  // sky
    .log_warn      = {249, 226, 175, 255},  // yellow
    .log_error     = {243, 139, 168, 255},  // red
    .log_fatal     = {245, 194, 231, 255},  // pink
    .log_debug     = {166, 173, 200, 255},  // subtext0
    .log_trace     = {127, 132, 156, 255},  // overlay0

    .debug_highlight = {166, 227, 161, 255}, // green

    .separator     = {69, 71, 90, 255},     // surface1
    .corner_radius = 4,
    .font_size     = 14,
};

static const gui_theme *active_theme = &dark_theme;

void gui_theme_set(const gui_theme *theme) {
    active_theme = theme ? theme : &dark_theme;
}

const gui_theme *gui_theme_get(void) {
    return active_theme;
}

const gui_theme *gui_theme_dark(void) {
    return &dark_theme;
}

const gui_theme *gui_theme_catppuccin(void) {
    return &catppuccin_theme;
}
