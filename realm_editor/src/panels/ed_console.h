#pragma once

#include "defines.h"
#include "host/host_console.h"

typedef struct ed_console {
    host_console core;
} ed_console;

void ed_console_init(ed_console *c);
void ed_console_shutdown(ed_console *c);
b8   ed_console_toggle(ed_console *c);
void ed_console_on_scroll(ed_console *c, f32 delta);
void ed_console_render(ed_console *c, f32 height, f32 dt);
