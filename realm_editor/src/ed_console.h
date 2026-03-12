#pragma once

#include "core/logger.h"
#include "defines.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text_input.h"

#define ED_CONSOLE_MAX_LINES 256
#define ED_CONSOLE_LINE_MAX  256

typedef struct ed_console_line {
    char text[ED_CONSOLE_LINE_MAX];
    u16 len;
    LOG_LEVEL level;
} ed_console_line;

typedef struct ed_console {
    ed_console_line lines[ED_CONSOLE_MAX_LINES];
    u32 head;
    u32 count;
    b8 visible;
    gui_scroll_state scroll;
    gui_text_input_state input;
} ed_console;

void ed_console_init(ed_console *c);
void ed_console_shutdown(ed_console *c);
b8   ed_console_toggle(ed_console *c);
void ed_console_on_scroll(ed_console *c, f32 delta);
void ed_console_render(ed_console *c, f32 dt);
