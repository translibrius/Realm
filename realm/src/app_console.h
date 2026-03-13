#pragma once

#include "defines.h"
#include "gui/gui_window.h"
#include "host/host_console.h"

typedef struct app_console {
    host_console core;
    gui_window_state window;
} app_console;

void app_console_init(app_console *c);
void app_console_shutdown(app_console *c);
b8   app_console_toggle(app_console *c);
void app_console_on_scroll(app_console *c, f32 delta);
void app_console_render(app_console *c, f32 dt);
