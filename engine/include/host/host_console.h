#pragma once

#include "core/logger.h"
#include "defines.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text_input.h"

#define HOST_CONSOLE_MAX_LINES 256
#define HOST_CONSOLE_LINE_MAX  256

typedef struct host_console_line {
    char text[HOST_CONSOLE_LINE_MAX];
    u16 len;
    LOG_LEVEL level;
} host_console_line;

typedef struct host_console {
    host_console_line lines[HOST_CONSOLE_MAX_LINES];
    u32 head;
    u32 count;
    b8 visible;
    gui_scroll_state scroll;
    gui_text_input_state input;
    i32 filter_level;              // dropdown index (0=All, 1..6=specific levels)
    gui_dropdown_state filter_dropdown;
} host_console;

// Prepared line data for rendering (arena-allocated, valid until end of frame).
typedef struct host_console_prepared_lines {
    char **ptrs;
    u16 *lens;
    void *colors; // Clay_Color* — void to avoid Clay include in header
    u32 count;
} host_console_prepared_lines;

REALM_API void host_console_init(host_console *c);
REALM_API void host_console_shutdown(host_console *c);
REALM_API b8   host_console_toggle(host_console *c);
REALM_API void host_console_on_scroll(host_console *c, f32 delta);

// Prepare line data into frame arena for rendering. Caller uses the result
// to feed log text into whatever layout they want. Respects filter_level.
REALM_API host_console_prepared_lines host_console_prepare_lines(host_console *c);

// Renders the log level filter dropdown. Call inside a horizontal layout row.
REALM_API void host_console_filter_dropdown(host_console *c, u16 font);
