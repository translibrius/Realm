#pragma once

#include "clay.h"
#include "defines.h"

typedef enum gui_sizing {
    GUI_SIZE_FIT     = 0, // default: shrink-to-fit content
    GUI_SIZE_GROW    = 1, // expand to fill parent
    GUI_SIZE_FIXED   = 2, // use the explicit width/height value
    GUI_SIZE_PERCENT = 3, // fraction of parent (width/height = 0.0 to 1.0)
} gui_sizing;

typedef struct gui_panel_cfg {
    Clay_Color color;        // background color (default: transparent)
    Clay_Color border_color; // border color
    f32 border_width;        // uniform border width (0 = no border)
    f32 corner_radius;       // uniform corner radius
    f32 padding;             // uniform padding (shorthand — overridden by pad_*)
    Clay_Padding pad;        // per-side padding (overrides uniform padding if any side > 0)
    f32 gap;                 // child gap
    f32 width;               // value for GUI_SIZE_FIXED (also auto-promotes if > 0 and sizing is FIT)
    f32 height;              // value for GUI_SIZE_FIXED (also auto-promotes if > 0 and sizing is FIT)
    gui_sizing width_sizing;  // default: GUI_SIZE_FIT
    gui_sizing height_sizing; // default: GUI_SIZE_FIT
    u8 align_x;              // 0 = left (default), CLAY_ALIGN_X_CENTER, CLAY_ALIGN_X_RIGHT
    u8 align_y;              // 0 = top (default), CLAY_ALIGN_Y_CENTER, CLAY_ALIGN_Y_BOTTOM
    b8 horizontal;           // false = top-to-bottom, true = left-to-right
} gui_panel_cfg;

REALM_API void gui_panel_begin(const gui_panel_cfg *cfg);
REALM_API void gui_panel_end(void);

// Shorthand: horizontal row with child gap. Call gui_row_end() to close.
REALM_API void gui_row(f32 gap);
REALM_API void gui_row_end(void);

// Shorthand: vertical column with child gap. Call gui_col_end() to close.
REALM_API void gui_col(f32 gap);
REALM_API void gui_col_end(void);

// Scoped variants — auto-close at end of block (like Clay's CLAY() macro).
// Usage: GUI_PANEL(&cfg) { children... }
//        GUI_ROW(8) { children... }
#define GUI_PANEL(cfg_ptr) for (b8 _gp = (gui_panel_begin(cfg_ptr), true); _gp; _gp = (gui_panel_end(), false))
#define GUI_ROW(gap_val)   for (b8 _gp = (gui_row(gap_val), true); _gp; _gp = (gui_row_end(), false))
#define GUI_COL(gap_val)   for (b8 _gp = (gui_col(gap_val), true); _gp; _gp = (gui_col_end(), false))

// Flexible spacer — grows to fill available space.
REALM_API void gui_spacer(void);

// Fixed-size spacer.
REALM_API void gui_spacer_fixed(f32 size);

// Visible horizontal line separator.
REALM_API void gui_separator(void);
