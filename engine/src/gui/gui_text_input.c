#include "gui/gui_text_input.h"

#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "engine.h"
#include "memory/arena.h"

#include <string.h>

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
    if (!s || !out || out_size < 2) {
        return 0;
    }

    s->cursor_blink += dt;
    if (s->cursor_blink > 1.0f) { s->cursor_blink -= 1.0f; }
    b8 show_caret = s->cursor_blink < 0.5f;

    u16 pos = 0;
    for (u16 i = 0; i < s->len && pos < out_size - 2; i++) {
        if (i == s->cursor && show_caret && pos < out_size - 2) {
            out[pos++] = '|';
        }
        out[pos++] = s->buf[i];
    }
    if (s->cursor == s->len && show_caret && pos < out_size - 1) {
        out[pos++] = '|';
    }
    out[pos] = '\0';
    return pos;
}

void gui_text_input_render(gui_text_input_state *state, f32 dt, const gui_text_input_render_cfg *cfg) {
    if (!state) return;

    Clay_Color bg_color     = {15, 15, 17, 255};
    Clay_Color text_color   = {200, 200, 205, 255};
    Clay_Color border_color = {60, 60, 65, 255};
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

    rl_arena *arena = rl_engine_get_frame_arena();
    char *display = rl_arena_push(arena, GUI_TEXT_INPUT_MAX + 2, false);
    u16 len = gui_text_input_display(state, dt, display, GUI_TEXT_INPUT_MAX + 2);

    gui_panel_begin(&(gui_panel_cfg){
        .color        = bg_color,
        .border_color = border_color,
        .border_width = border_width,
        .width_sizing = GUI_SIZE_GROW,
        .height       = height,
        .padding      = padding,
    });
    gui_textn(display, len, &(gui_text_cfg){.color = text_color, .size = font_size, .font = font});
    gui_panel_end();
}
