#include "gui/gui_text.h"

#include <string.h>

void gui_text(const char *str, const gui_text_cfg *cfg) {
    if (!str) return;

    u16 font = 0;
    u16 size = 14;
    Clay_Color color = {255, 255, 255, 255};

    if (cfg) {
        if (cfg->size > 0) size = cfg->size;
        font = cfg->font;
        if (cfg->color.a > 0) color = cfg->color;
    }

    Clay_String text = {.length = (i32)strlen(str), .chars = str};
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.textColor = color, .fontSize = size, .fontId = font}));
}

void gui_textn(const char *str, u16 len, const gui_text_cfg *cfg) {
    if (!str || len == 0) return;

    u16 font = 0;
    u16 size = 14;
    Clay_Color color = {255, 255, 255, 255};

    if (cfg) {
        if (cfg->size > 0) size = cfg->size;
        font = cfg->font;
        if (cfg->color.a > 0) color = cfg->color;
    }

    Clay_String text = {.length = (i32)len, .chars = str};
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.textColor = color, .fontSize = size, .fontId = font}));
}
