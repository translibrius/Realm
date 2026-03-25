#include "gui/gui_text_input.h"

#include "gui/gui.h"
#include "gui/gui_focus.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "engine.h"
#include "memory/arena.h"
#include "platform/input.h"

#include "gui_internal.h"

#include <string.h>

static void ensure_id(gui_text_input_state *s) {
    if (s->_id == 0) s->_id = gui__next_id();
}

b8 gui_text_input_handle_key(gui_text_input_state *s, input_key *key) {
    if (!s || !key || !key->pressed) {
        return false;
    }

    switch (key->key) {
    case KEY_BACKSPACE:
        if (s->cursor > 0) {
            memmove(&s->buf[s->cursor - 1], &s->buf[s->cursor], s->len - s->cursor);
            s->cursor--;
            s->len--;
            s->buf[s->len] = '\0';
        }
        break;
    case KEY_LEFT:
        if (s->cursor > 0) { s->cursor--; }
        break;
    case KEY_RIGHT:
        if (s->cursor < s->len) { s->cursor++; }
        break;
    case KEY_ENTER:
        s->cursor_blink = 0;
        return true;
    default:
        break;
    }

    s->cursor_blink = 0;
    return false;
}

void gui_text_input_handle_char(gui_text_input_state *s, input_char *ch) {
    if (!s || !ch) {
        return;
    }
    if (s->_skip_next_char) {
        s->_skip_next_char = false;
        return;
    }
    if (ch->codepoint < 32 || ch->codepoint > 126) {
        return;
    }
    if (s->len >= GUI_TEXT_INPUT_MAX - 1) {
        return;
    }

    memmove(&s->buf[s->cursor + 1], &s->buf[s->cursor], s->len - s->cursor);
    s->buf[s->cursor] = (char)ch->codepoint;
    s->cursor++;
    s->len++;
    s->buf[s->len] = '\0';
    s->cursor_blink = 0;
}

u16 gui_text_input_display(gui_text_input_state *s, f32 dt, char *out, u16 out_size) {
    (void)dt;
    if (!s || !out || out_size < 2) {
        return 0;
    }

    ensure_id(s);

    u16 copy_len = s->len;
    if (copy_len >= out_size) copy_len = out_size - 1;
    memcpy(out, s->buf, copy_len);
    out[copy_len] = '\0';
    return copy_len;
}

void gui_text_input_render(gui_text_input_state *state, f32 dt, const gui_text_input_render_cfg *cfg) {
    if (!state) return;

    ensure_id(state);

    const gui_theme *th     = gui_theme_get();
    Clay_Color bg_color     = th->bg_input;
    Clay_Color text_color   = th->text;
    Clay_Color border_color = th->border;
    f32 border_width = 1;
    f32 padding      = 8;
    f32 height       = 28;
    u16 font         = 0;
    u16 font_size    = 13;

    if (cfg) {
        if (cfg->bg_color.a > 0)     bg_color     = cfg->bg_color;
        if (cfg->text_color.a > 0)   text_color   = cfg->text_color;
        if (cfg->border_color.a > 0) border_color = cfg->border_color;
        if (cfg->border_width > 0)   border_width = cfg->border_width;
        if (cfg->padding > 0)        padding      = cfg->padding;
        if (cfg->height > 0)         height       = cfg->height;
        font      = cfg->font;
        if (cfg->font_size > 0)      font_size    = cfg->font_size;
    }

    b8 focused = gui_focus_is(state->_id);

    rl_arena *arena = rl_engine_get_frame_arena();
    char *display = rl_arena_push(arena, GUI_TEXT_INPUT_MAX + 2, false);
    u16 len = gui_text_input_display(state, dt, display, GUI_TEXT_INPUT_MAX + 2);

    gui_panel_begin(&(gui_panel_cfg){
        .color        = bg_color,
        .border_color = focused ? th->accent : border_color,
        .border_width = border_width,
        .width_sizing = GUI_SIZE_GROW,
        .height       = height,
        .padding      = padding,
    });

    // Click to focus
    if (Clay_Hovered() && input_mouse_pressed(MOUSE_LEFT)) {
        gui_focus_set_input(state->_id, GUI_INPUT_TEXT, state);
    }

    gui_textn(display, len, &(gui_text_cfg){.color = text_color, .size = font_size, .font = font});

    // Floating pixel cursor (thin blinking line between characters)
    if (focused) {
        state->cursor_blink += dt;
        if (state->cursor_blink > 1.0f) state->cursor_blink -= 1.0f;
        if (state->cursor_blink < 0.5f) {
            f32 cursor_x = gui_measure_text_width(state->buf, state->cursor, font, font_size);
            f32 caret_h = (f32)font_size + 4.0f;
            Clay__OpenElement();
            Clay__ConfigureOpenElement((Clay_ElementDeclaration){
                .layout = {.sizing = {.width = CLAY_SIZING_FIXED(1.0f),
                                      .height = CLAY_SIZING_FIXED(caret_h)}},
                .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                             .attachPoints = {.parent = CLAY_ATTACH_POINT_LEFT_CENTER,
                                              .element = CLAY_ATTACH_POINT_LEFT_CENTER},
                             .offset = {padding + cursor_x - 0.5f, 0},
                             .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                             .zIndex = 10},
                .backgroundColor = text_color,
            });
            Clay__CloseElement();
        }
    }

    gui_panel_end();
}
