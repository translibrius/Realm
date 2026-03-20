#include "gui/gui_theme.h"

#include "renderer/renderer_frontend.h"

static const gui_theme dark_theme = {
    .bg            = {25, 25, 28, 230},
    .bg_secondary  = {38, 38, 44, 255},
    .bg_titlebar   = {20, 20, 23, 255},
    .bg_elevated   = {42, 42, 48, 255},
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
    .viewport_bg   = {51, 51, 51, 255},   // 0.2 gray — matches legacy RL_CLEAR_COLOR
    .corner_radius = 4,
    .font_size     = 14,
};

// Catppuccin Mocha — https://catppuccin.com/palette
static const gui_theme catppuccin_theme = {
    .bg            = {30, 30, 46, 230},     // base
    .bg_secondary  = {49, 50, 68, 255},     // surface0
    .bg_titlebar   = {24, 24, 37, 255},     // mantle
    .bg_elevated   = {49, 50, 68, 255},     // surface0
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
    .viewport_bg   = {24, 24, 37, 255},     // mantle
    .corner_radius = 4,
    .font_size     = 14,
};

// Dracula — https://draculatheme.com/contribute
static const gui_theme dracula_theme = {
    .bg            = {40, 42, 54, 230},     // background #282a36
    .bg_secondary  = {68, 71, 90, 255},     // current line #44475a
    .bg_titlebar   = {33, 34, 44, 255},     // #21222c
    .bg_elevated   = {55, 57, 72, 255},     // between bg and current line
    .bg_input      = {33, 34, 44, 255},     // darker bg
    .bg_overlay    = {22, 23, 36, 180},     // deep bg

    .accent        = {189, 147, 249, 200},  // purple #bd93f9
    .accent_hover  = {189, 147, 249, 255},  // purple full

    .text          = {248, 248, 242, 255},  // foreground #f8f8f2
    .text_dim      = {98, 114, 164, 255},   // comment #6272a4

    .border        = {68, 71, 90, 255},     // current line #44475a

    .control       = {55, 57, 72, 255},     // between bg and current line
    .control_hover = {68, 71, 90, 255},     // current line
    .control_press = {45, 47, 60, 255},     // pressed
    .control_thumb = {248, 248, 242, 255},  // foreground

    .danger        = {255, 85, 85, 255},    // red #ff5555
    .danger_press  = {255, 110, 110, 255},  // brighter red

    .log_info      = {139, 233, 253, 255},  // cyan #8be9fd
    .log_warn      = {241, 250, 140, 255},  // yellow #f1fa8c
    .log_error     = {255, 85, 85, 255},    // red #ff5555
    .log_fatal     = {255, 121, 198, 255},  // pink #ff79c6
    .log_debug     = {98, 114, 164, 255},   // comment #6272a4
    .log_trace     = {78, 94, 144, 255},    // dimmer comment

    .debug_highlight = {80, 250, 123, 255}, // green #50fa7b


    .separator     = {68, 71, 90, 255},     // current line
    .viewport_bg   = {33, 34, 44, 255},     // darker bg
    .corner_radius = 4,
    .font_size     = 14,
};

// Gruvbox Dark — https://github.com/morhetz/gruvbox
static const gui_theme gruvbox_theme = {
    .bg            = {40, 40, 40, 230},     // bg0 #282828
    .bg_secondary  = {60, 56, 54, 255},     // bg1 #3c3836
    .bg_titlebar   = {29, 32, 33, 255},     // bg0_h (hard dark)
    .bg_elevated   = {60, 56, 54, 255},     // bg1 #3c3836
    .bg_input      = {29, 32, 33, 255},     // bg0_h (hard)
    .bg_overlay    = {22, 22, 22, 180},     // deep bg

    .accent        = {69, 133, 136, 200},   // blue #458588
    .accent_hover  = {131, 165, 152, 255},  // bright_blue #83a598

    .text          = {235, 219, 178, 255},  // fg1 #ebdbb2
    .text_dim      = {168, 153, 132, 255},  // fg4 #a89984

    .border        = {80, 73, 69, 255},     // bg2 #504945

    .control       = {60, 56, 54, 255},     // bg1 #3c3836
    .control_hover = {80, 73, 69, 255},     // bg2 #504945
    .control_press = {50, 48, 47, 255},     // between bg0 and bg1
    .control_thumb = {213, 196, 161, 255},  // fg2 #d5c4a1

    .danger        = {251, 73, 52, 255},    // bright_red #fb4934
    .danger_press  = {204, 36, 29, 255},    // neutral_red #cc241d

    .log_info      = {131, 165, 152, 255},  // bright_blue #83a598
    .log_warn      = {250, 189, 47, 255},   // bright_yellow #fabd2f
    .log_error     = {251, 73, 52, 255},    // bright_red #fb4934
    .log_fatal     = {211, 134, 155, 255},  // bright_purple #d3869b
    .log_debug     = {168, 153, 132, 255},  // fg4 #a89984
    .log_trace     = {124, 111, 100, 255},  // bg4 #7c6f64

    .debug_highlight = {184, 187, 38, 255}, // bright_green #b8bb26


    .separator     = {80, 73, 69, 255},     // bg2 #504945
    .viewport_bg   = {29, 32, 33, 255},     // bg0_h (hard dark)
    .corner_radius = 4,
    .font_size     = 14,
};

// Nord — https://www.nordtheme.com
static const gui_theme nord_theme = {
    .bg            = {46, 52, 64, 230},     // nord0 #2e3440
    .bg_secondary  = {59, 66, 82, 255},     // nord1 #3b4252
    .bg_titlebar   = {39, 44, 54, 255},     // darker polar night
    .bg_elevated   = {67, 76, 94, 255},     // nord2 #434c5e
    .bg_input      = {39, 44, 54, 255},     // darker than nord0
    .bg_overlay    = {30, 34, 42, 180},     // deep bg

    .accent        = {136, 192, 208, 200},  // nord8 #88c0d0
    .accent_hover  = {136, 192, 208, 255},  // nord8 full

    .text          = {216, 222, 233, 255},  // nord4 #d8dee9
    .text_dim      = {129, 140, 160, 255},  // lightened nord3 for readability

    .border        = {67, 76, 94, 255},     // nord2 #434c5e

    .control       = {52, 59, 73, 255},     // between nord0 and nord1
    .control_hover = {76, 86, 106, 255},    // nord3 #4c566a
    .control_press = {46, 52, 64, 255},     // nord0
    .control_thumb = {229, 233, 240, 255},  // nord5 #e5e9f0

    .danger        = {191, 97, 106, 255},   // nord11 #bf616a
    .danger_press  = {208, 135, 112, 255},  // nord12 #d08770

    .log_info      = {136, 192, 208, 255},  // nord8 #88c0d0
    .log_warn      = {235, 203, 139, 255},  // nord13 #ebcb8b
    .log_error     = {191, 97, 106, 255},   // nord11 #bf616a
    .log_fatal     = {180, 142, 173, 255},  // nord15 #b48ead
    .log_debug     = {129, 140, 160, 255},  // lightened nord3
    .log_trace     = {97, 110, 136, 255},   // mid nord3

    .debug_highlight = {163, 190, 140, 255}, // nord14 #a3be8c


    .separator     = {67, 76, 94, 255},     // nord2 #434c5e
    .viewport_bg   = {39, 44, 54, 255},     // darker polar night
    .corner_radius = 4,
    .font_size     = 14,
};

// Tokyo Night — https://github.com/enkia/tokyo-night-vscode-theme
static const gui_theme tokyonight_theme = {
    .bg            = {26, 27, 38, 230},     // bg #1a1b26
    .bg_secondary  = {31, 35, 53, 255},     // dark bg #1f2335 (sidebar/panels)
    .bg_titlebar   = {22, 22, 30, 255},     // bg_dark #16161e
    .bg_elevated   = {41, 46, 66, 255},     // bg_highlight #292e42
    .bg_input      = {22, 22, 30, 255},     // bg_dark #16161e
    .bg_overlay    = {16, 16, 24, 180},     // deep bg

    .accent        = {122, 162, 247, 200},  // blue #7aa2f7
    .accent_hover  = {122, 162, 247, 255},  // blue full

    .text          = {192, 202, 245, 255},  // fg #c0caf5
    .text_dim      = {86, 95, 137, 255},    // comment #565f89

    .border        = {41, 46, 66, 255},     // bg_highlight #292e42

    .control       = {36, 40, 58, 255},     // between sidebar and highlight
    .control_hover = {45, 50, 72, 255},     // above bg_highlight
    .control_press = {28, 31, 46, 255},     // darker
    .control_thumb = {192, 202, 245, 255},  // fg

    .danger        = {247, 118, 142, 255},  // red #f7768e
    .danger_press  = {255, 150, 170, 255},  // brighter red

    .log_info      = {42, 195, 222, 255},   // cyan #2ac3de
    .log_warn      = {224, 175, 104, 255},  // yellow #e0af68
    .log_error     = {247, 118, 142, 255},  // red #f7768e
    .log_fatal     = {187, 154, 247, 255},  // magenta #bb9af7
    .log_debug     = {86, 95, 137, 255},    // comment #565f89
    .log_trace     = {65, 72, 104, 255},    // terminal_black #414868

    .debug_highlight = {158, 206, 106, 255}, // green #9ece6a


    .separator     = {36, 40, 58, 255},     // subtle divider
    .viewport_bg   = {22, 22, 30, 255},     // bg_dark
    .corner_radius = 4,
    .font_size     = 14,
};

// One Dark — https://github.com/atom/one-dark-syntax
static const gui_theme onedark_theme = {
    .bg            = {40, 44, 52, 230},     // bg #282c34
    .bg_secondary  = {53, 59, 69, 255},     // gutter/raised
    .bg_titlebar   = {33, 37, 43, 255},     // darker bg #21252b
    .bg_elevated   = {53, 59, 69, 255},     // gutter level
    .bg_input      = {33, 37, 43, 255},     // darker bg
    .bg_overlay    = {24, 26, 32, 180},     // deep bg

    .accent        = {97, 175, 239, 200},   // blue #61afef
    .accent_hover  = {97, 175, 239, 255},   // blue full

    .text          = {171, 178, 191, 255},  // fg #abb2bf
    .text_dim      = {92, 99, 112, 255},    // comment/gutter #5c6370

    .border        = {75, 82, 99, 255},     // gutter #4b5263

    .control       = {53, 59, 69, 255},     // raised surface
    .control_hover = {75, 82, 99, 255},     // gutter
    .control_press = {44, 49, 58, 255},     // between bg and raised
    .control_thumb = {171, 178, 191, 255},  // fg

    .danger        = {224, 108, 117, 255},  // red #e06c75
    .danger_press  = {240, 130, 140, 255},  // brighter red

    .log_info      = {86, 182, 194, 255},   // cyan #56b6c2
    .log_warn      = {229, 192, 123, 255},  // yellow #e5c07b
    .log_error     = {224, 108, 117, 255},  // red #e06c75
    .log_fatal     = {198, 120, 221, 255},  // magenta #c678dd
    .log_debug     = {92, 99, 112, 255},    // comment #5c6370
    .log_trace     = {75, 82, 99, 255},     // gutter #4b5263

    .debug_highlight = {152, 195, 121, 255}, // green #98c379


    .separator     = {75, 82, 99, 255},     // gutter
    .viewport_bg   = {33, 37, 43, 255},     // darker bg
    .corner_radius = 4,
    .font_size     = 14,
};

// Rose Pine — https://rosepinetheme.com
static const gui_theme rosepine_theme = {
    .bg            = {25, 23, 36, 230},     // base #191724
    .bg_secondary  = {31, 29, 46, 255},     // surface #1f1d2e
    .bg_titlebar   = {21, 19, 30, 255},     // darker base
    .bg_elevated   = {38, 35, 58, 255},     // overlay #26233a
    .bg_input      = {21, 19, 30, 255},     // darker than base
    .bg_overlay    = {16, 15, 24, 180},     // deep bg

    .accent        = {196, 167, 231, 200},  // iris #c4a7e7
    .accent_hover  = {196, 167, 231, 255},  // iris full

    .text          = {224, 222, 244, 255},  // text #e0def4
    .text_dim      = {110, 106, 134, 255},  // muted #6e6a86

    .border        = {38, 35, 58, 255},     // overlay #26233a

    .control       = {31, 29, 46, 255},     // surface #1f1d2e
    .control_hover = {48, 45, 72, 255},     // above overlay for visible hover
    .control_press = {26, 24, 38, 255},     // between base and surface
    .control_thumb = {144, 140, 170, 255},  // subtle #908caa

    .danger        = {235, 111, 146, 255},  // love #eb6f92
    .danger_press  = {245, 140, 170, 255},  // brighter love

    .log_info      = {156, 207, 216, 255},  // foam #9ccfd8
    .log_warn      = {246, 193, 119, 255},  // gold #f6c177
    .log_error     = {235, 111, 146, 255},  // love #eb6f92
    .log_fatal     = {196, 167, 231, 255},  // iris #c4a7e7
    .log_debug     = {110, 106, 134, 255},  // muted #6e6a86
    .log_trace     = {86, 82, 110, 255},    // dimmer muted

    .debug_highlight = {49, 116, 143, 255}, // pine #31748f


    .separator     = {38, 35, 58, 255},     // overlay #26233a
    .viewport_bg   = {21, 19, 30, 255},     // darker base
    .corner_radius = 4,
    .font_size     = 14,
};

static const gui_theme *active_theme = &dark_theme;

void gui_theme_set(const gui_theme *theme) {
    active_theme = theme ? theme : &dark_theme;
    renderer_set_clear_color(
        active_theme->viewport_bg.r / 255.0f, active_theme->viewport_bg.g / 255.0f,
        active_theme->viewport_bg.b / 255.0f, active_theme->viewport_bg.a / 255.0f);
}

const gui_theme *gui_theme_get(void) {
    return active_theme;
}

const gui_theme *gui_theme_dark(void)       { return &dark_theme; }
const gui_theme *gui_theme_catppuccin(void) { return &catppuccin_theme; }
const gui_theme *gui_theme_dracula(void)    { return &dracula_theme; }
const gui_theme *gui_theme_gruvbox(void)    { return &gruvbox_theme; }
const gui_theme *gui_theme_nord(void)       { return &nord_theme; }
const gui_theme *gui_theme_tokyonight(void) { return &tokyonight_theme; }
const gui_theme *gui_theme_onedark(void)    { return &onedark_theme; }
const gui_theme *gui_theme_rosepine(void)   { return &rosepine_theme; }
