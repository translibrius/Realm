#include "gui/gui_text.h"

#include "engine.h"
#include "gui/gui_theme.h"
#include "memory/arena.h"
#include "util/str.h"

#include <stdarg.h>
#include <string.h>

void gui_text(const char *str, const gui_text_cfg *cfg) {
    if (!str) return;

    u16 font = 0;
    u16 size = 14;
    Clay_Color color = gui_theme_get()->text;
    u16 max_chars = 0;
    b8 truncate_head = false;

    if (cfg) {
        if (cfg->size > 0) size = cfg->size;
        font = cfg->font;
        if (cfg->color.a > 0) color = cfg->color;
        max_chars = cfg->max_chars;
        truncate_head = cfg->truncate_head;
    }

    const char *display = str;
    u32 len = cstr_len(str);

    if (max_chars > 3 && len > max_chars) {
        rl_arena *frame = rl_engine_get_frame_arena();
        if (truncate_head) {
            // "...tail"
            u32 tail_len = max_chars - 3;
            display = cstr_format(frame, "...%s", str + len - tail_len);
            len = max_chars;
        } else {
            // "head..."
            char *buf = rl_arena_push(frame, max_chars + 1, 1);
            mem_copy(buf, str, max_chars - 3);
            buf[max_chars - 3] = '.';
            buf[max_chars - 2] = '.';
            buf[max_chars - 1] = '.';
            buf[max_chars] = '\0';
            display = buf;
            len = max_chars;
        }
    }

    Clay_String text = {.length = (i32)len, .chars = display};
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.textColor = color, .fontSize = size, .fontId = font}));
}

void gui_textn(const char *str, u16 len, const gui_text_cfg *cfg) {
    if (!str || len == 0) return;

    u16 font = 0;
    u16 size = 14;
    Clay_Color color = gui_theme_get()->text;

    if (cfg) {
        if (cfg->size > 0) size = cfg->size;
        font = cfg->font;
        if (cfg->color.a > 0) color = cfg->color;
    }

    Clay_String text = {.length = (i32)len, .chars = str};
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.textColor = color, .fontSize = size, .fontId = font}));
}

void gui_textf(const gui_text_cfg *cfg, const char *fmt, ...) {
    if (!fmt) return;
    rl_arena *arena = rl_engine_get_frame_arena();
    va_list args;
    va_start(args, fmt);
    char *str = cstr_format_va(arena, fmt, args);
    va_end(args);
    gui_text(str, cfg);
}
