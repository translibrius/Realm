#include "gui/gui_number_input.h"

#include "engine.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "memory/arena.h"
#include "platform/input.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

static f32 clampf(f32 v, f32 lo, f32 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

b8 gui_number_input(gui_number_input_state *state, const gui_number_input_cfg *cfg, f32 dt) {
    if (!state) return false;

    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    f32 step   = (cfg && cfg->step > 0) ? cfg->step : 0.01f;
    const char *fmt = (cfg && cfg->format) ? cfg->format : "%.2f";
    f32 width  = (cfg && cfg->width > 0)  ? cfg->width  : 55;
    f32 height = (cfg && cfg->height > 0) ? cfg->height : 20;

    // min/max: both zero means "no limit", otherwise use as-is
    f32 mn = -FLT_MAX;
    f32 mx =  FLT_MAX;
    if (cfg && (cfg->min != 0 || cfg->max != 0)) {
        mn = cfg->min;
        mx = cfg->max;
    }

    const gui_theme *t = gui_theme_get();
    f32 old_value = state->value;

    Clay_ElementId eid = CLAY_IDI("GuiNumberInput", state->_id);
    Clay_ElementData ed = Clay_GetElementData(eid);

    Clay__OpenElementWithId(eid);
    b8 hovered = Clay_Hovered();
    Clay_Color bg = state->editing ? t->bg_input : t->control;
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height)},
            .padding = {4, 4, 0, 0},
            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = bg,
        .cornerRadius = CLAY_CORNER_RADIUS(3),
    });

    // Interaction
    if (!state->editing && !state->dragging && hovered && input_mouse_pressed(MOUSE_LEFT)) {
        state->dragging = true;
        state->drag_start_value = state->value;
        vec2 mouse;
        input_get_mouse_position(mouse);
        state->drag_origin_x = mouse[0];
    }

    if (state->dragging) {
        if (!input_is_mouse_down(MOUSE_LEFT)) {
            // Mouse released — check if it was a click (< 2px movement)
            vec2 mouse;
            input_get_mouse_position(mouse);
            f32 dx = mouse[0] - state->drag_origin_x;
            if (dx < 0) dx = -dx;

            if (dx < 2.0f) {
                // Click → enter editing mode
                state->editing = true;
                state->cursor_blink = 0;
                snprintf(state->buf, sizeof(state->buf), fmt, (f64)state->value);
                state->len = (u16)strlen(state->buf);
                state->cursor = state->len;
            }
            state->dragging = false;
        } else if (ed.found) {
            // Dragging — scrub value
            vec2 mouse;
            input_get_mouse_position(mouse);
            f32 dx = mouse[0] - state->drag_origin_x;
            state->value = clampf(state->drag_start_value + dx * step, mn, mx);
        }
    }

    // Render text
    rl_arena *arena = rl_engine_get_frame_arena();
    u16 font = 0;

    if (state->editing) {
        // Show buffer with blinking caret
        state->cursor_blink += dt;
        if (state->cursor_blink > 1.0f) state->cursor_blink -= 1.0f;
        b8 show_caret = state->cursor_blink < 0.5f;

        char *display = rl_arena_push(arena, 64, false);
        u16 pos = 0;
        for (u16 i = 0; i < state->len && pos < 60; i++) {
            if (i == state->cursor && show_caret) display[pos++] = '|';
            display[pos++] = state->buf[i];
        }
        if (state->cursor == state->len && show_caret && pos < 62) {
            display[pos++] = '|';
        }
        display[pos] = '\0';
        gui_textn(display, pos, &(gui_text_cfg){.color = t->text, .size = 12, .font = font});
    } else {
        char *display = rl_arena_push(arena, 32, false);
        snprintf(display, 32, fmt, (f64)state->value);
        u16 dlen = (u16)strlen(display);
        Clay_Color tc = hovered ? t->text : t->text_dim;
        gui_textn(display, dlen, &(gui_text_cfg){.color = tc, .size = 12, .font = font});
    }

    Clay__CloseElement();

    return state->value != old_value;
}

b8 gui_number_input_handle_key(gui_number_input_state *state, input_key *key) {
    if (!state || !state->editing || !key || !key->pressed) return false;

    switch (key->key) {
    case KEY_BACKSPACE:
        if (state->cursor > 0) {
            memmove(&state->buf[state->cursor - 1], &state->buf[state->cursor],
                    state->len - state->cursor);
            state->cursor--;
            state->len--;
            state->buf[state->len] = '\0';
        }
        break;
    case KEY_LEFT:
        if (state->cursor > 0) state->cursor--;
        break;
    case KEY_RIGHT:
        if (state->cursor < state->len) state->cursor++;
        break;
    case KEY_ENTER:
        // Confirm: parse and exit editing
        return true;
    case KEY_ESCAPE:
        // Cancel: revert to drag_start_value and exit editing
        state->value = state->drag_start_value;
        state->editing = false;
        return false;
    default:
        break;
    }

    state->cursor_blink = 0;
    return false;
}

void gui_number_input_handle_char(gui_number_input_state *state, input_char *ch) {
    if (!state || !state->editing || !ch) return;

    char c = (char)ch->codepoint;
    // Only accept digits, dot, minus
    b8 valid = (c >= '0' && c <= '9') || c == '.' || c == '-';
    if (!valid) return;
    if (state->len >= 30) return;

    memmove(&state->buf[state->cursor + 1], &state->buf[state->cursor],
            state->len - state->cursor);
    state->buf[state->cursor] = c;
    state->cursor++;
    state->len++;
    state->buf[state->len] = '\0';
    state->cursor_blink = 0;
}
