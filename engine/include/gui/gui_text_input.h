#pragma once

#include "clay.h"
#include "defines.h"
#include "platform/input.h"

#define GUI_TEXT_INPUT_MAX 256

typedef struct gui_text_input_state {
    char buf[GUI_TEXT_INPUT_MAX];
    u16 len;
    u16 cursor;
    f32 cursor_blink;
} gui_text_input_state;

// Process a key event. Returns true if Enter was pressed (submit).
REALM_API b8 gui_text_input_handle_key(gui_text_input_state *state, input_key *key);

// Process a char event (insert printable character).
REALM_API void gui_text_input_handle_char(gui_text_input_state *state, input_char *ch);

// Build display string with blinking caret into out_buf. Returns length written.
REALM_API u16 gui_text_input_display(gui_text_input_state *state, f32 dt, char *out_buf, u16 out_buf_size);

// Self-rendering text input with background and blinking caret.
// cfg is optional (NULL = defaults). Uses frame arena internally.
typedef struct gui_text_input_render_cfg {
    Clay_Color bg_color;   // input background (default: dark)
    Clay_Color text_color; // text color (default: light gray)
    Clay_Color border_color; // border color (default: dim)
    f32 border_width;      // 0 = no border (default: 1)
    f32 padding;           // default: 8
    f32 height;            // default: 28
    u16 font;
    u16 font_size;         // default: 13
} gui_text_input_render_cfg;

REALM_API void gui_text_input_render(gui_text_input_state *state, f32 dt, const gui_text_input_render_cfg *cfg);
