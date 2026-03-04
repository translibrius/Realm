#include "gui/gui_theme.h"

static const gui_theme dark_theme = {
    .bg            = {20, 20, 22, 230},
    .bg_secondary  = {35, 35, 40, 255},
    .bg_input      = {15, 15, 17, 255},

    .accent        = {70, 130, 220, 255},
    .accent_hover  = {90, 150, 240, 255},

    .text          = {220, 220, 225, 255},
    .text_dim      = {140, 140, 145, 255},

    .border        = {60, 60, 65, 255},

    .control       = {50, 50, 55, 255},
    .control_hover = {65, 65, 70, 255},
    .control_thumb = {230, 230, 235, 255},

    .separator     = {60, 60, 65, 255},
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
