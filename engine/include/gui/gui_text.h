#pragma once

#include "clay.h"
#include "defines.h"

typedef struct gui_text_cfg {
    Clay_Color color; // default: white (255,255,255,255)
    u16 size;         // font size in px (default: 14)
    u16 font;         // Clay font index (default: 0)
} gui_text_cfg;

// Null-terminated string.
REALM_API void gui_text(const char *str, const gui_text_cfg *cfg);

// Non-null-terminated string with explicit length.
REALM_API void gui_text_dynamic(const char *str, u16 len, const gui_text_cfg *cfg);
