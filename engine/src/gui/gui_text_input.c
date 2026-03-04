#include "gui/gui_text_input.h"

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
