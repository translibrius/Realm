#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_text_cfg {
    Clay_Color color; // default: white (255,255,255,255)
    u16 size;         // font size in px (default: 14)
    u16 font;         // Clay font index (default: 0)
    u16 max_chars;    // 0 = unlimited, >0 = truncate with "..." (from end or start)
    b8  truncate_head; // false = "long te..." (default), true = "...ng text"
} gui_text_cfg;

// Null-terminated string.
REALM_API void gui_text(const char *str, const gui_text_cfg *cfg);

// String with explicit length (not required to be null-terminated).
REALM_API void gui_textn(const char *str, u16 len, const gui_text_cfg *cfg);

// Formatted text (printf-style). Uses the frame arena for the formatted string.
REALM_API void gui_textf(const gui_text_cfg *cfg, const char *fmt, ...);
