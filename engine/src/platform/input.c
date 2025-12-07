#include "input.h"

#include "core/event.h"
#include "memory/memory.h"

typedef struct mouse_state {
    i16 x, y;
    b8 pressed[MOUSE_MAX_BUTTONS];
} mouse_state;

typedef struct keyboard_state {
    b8 pressed[KEY_MAX_KEYS];
} keyboard_state;

typedef struct input_state {
    mouse_state mouse_now;
    mouse_state mouse_prev;
    keyboard_state keyboard_now;
    keyboard_state keyboard_prev;
} input_state;

static input_state state;

void input_update() {
    rl_copy(&state.keyboard_now, &state.keyboard_prev, sizeof(keyboard_state));
    rl_copy(&state.mouse_now, &state.mouse_prev, sizeof(mouse_state));
}

void input_process_key(KEYBOARD_KEY key, b8 is_pressed) {
    if (state.keyboard_now.pressed[key] != is_pressed) {
        state.keyboard_now.pressed[key] = is_pressed;

        event_fire(EVENT_KEY_PRESS, &(input_key){key, is_pressed});
    }
}

void input_process_mouse_button(MOUSE_BUTTON button, b8 is_pressed) {
    if (state.mouse_now.pressed[button] != is_pressed) {
        state.mouse_now.pressed[button] = is_pressed;

        event_fire(EVENT_MOUSE_CLICK, &(input_mouse_button){button, is_pressed});
    }
}

void input_process_mouse_move(i32 position_x, i32 position_y) {
    if (state.mouse_now.x != position_x || state.mouse_now.y != position_y) {
        state.mouse_now.x = position_x;
        state.mouse_now.y = position_y;

        event_fire(EVENT_MOUSE_MOVE, &(input_mouse_move){position_x, position_y});
    }
}

void input_process_mouse_scroll(i32 delta) {
    event_fire(EVENT_MOUSE_SCROLL, &(input_mouse_scroll){delta});
}