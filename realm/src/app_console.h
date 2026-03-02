#pragma once

#include "core/logger.h"
#include "defines.h"

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
} app_console;

void app_console_init(app_console *c);
void app_console_shutdown(app_console *c);
void app_console_toggle(app_console *c);
void app_console_render(app_console *c);
