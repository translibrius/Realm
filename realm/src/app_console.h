#pragma once

#include "core/logger.h"
#include "defines.h"
#include "gui/gui_clay.h"

#define APP_CONSOLE_MAX_LINES 256
#define APP_CONSOLE_LINE_MAX  256

typedef struct app_console_line {
    char text[APP_CONSOLE_LINE_MAX];
    u16 len;
    LOG_LEVEL level;
} app_console_line;

typedef struct app_console {
    app_console_line lines[APP_CONSOLE_MAX_LINES];
    u32 head;
    u32 count;
    b8 visible;
    b8 auto_scroll;
    b8 dragging;
    f32 pos_x;
    f32 pos_y;
    gui_text_input_state input;
} app_console;

// Called from scroll event — does NOT consume it, just tracks auto-scroll state
void app_console_on_scroll(app_console *c, f32 delta);

void app_console_init(app_console *c);
void app_console_shutdown(app_console *c);
// Returns true if console was just opened (false if closed).
b8   app_console_toggle(app_console *c);
void app_console_render(app_console *c, f32 dt);
