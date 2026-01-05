#include "input.h"

#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"

typedef struct mouse_state {
    i16 x, y;
    i16 dx, dy;
    b8 pressed[MOUSE_MAX_BUTTONS];
} mouse_state;

typedef struct keyboard_state {
    b8 pressed[KEY_MAX_KEYS];
} keyboard_state;

typedef struct input_state {
    INPUT_MODE input_mode;
    mouse_state mouse_now;
    mouse_state mouse_prev;
    keyboard_state keyboard_now;
    keyboard_state keyboard_prev;
} input_state;

static input_state state;

void input_system_init() {
    state.input_mode = INPUT_MODE_UI;
}

void input_update() {
    rl_copy(&state.keyboard_now, &state.keyboard_prev, sizeof(keyboard_state));
    rl_copy(&state.mouse_now, &state.mouse_prev, sizeof(mouse_state));

    state.mouse_now.dx = 0;
    state.mouse_now.dy = 0;
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
        state.mouse_now.dx += position_x - state.mouse_now.x;
        state.mouse_now.dy += position_y - state.mouse_now.y;

        state.mouse_now.x = position_x;
        state.mouse_now.y = position_y;

        event_fire(EVENT_MOUSE_MOVE, &(input_mouse_move){position_x, position_y});
    }
}

void input_process_mouse_scroll(i32 delta) {
    event_fire(EVENT_MOUSE_SCROLL, &(input_mouse_scroll){delta});
}

b8 input_is_key_down(KEYBOARD_KEY key) {
    return state.keyboard_now.pressed[key];
}

b8 input_key_pressed(KEYBOARD_KEY key) {
    return !state.keyboard_prev.pressed[key] && state.keyboard_now.pressed[key];
}

b8 input_key_released(KEYBOARD_KEY key) {
    return state.keyboard_prev.pressed[key] && !state.keyboard_now.pressed[key];
}

b8 input_is_mouse_down(MOUSE_BUTTON button) {
    return state.mouse_now.pressed[button];
}

b8 input_mouse_pressed(MOUSE_BUTTON button) {
    return !state.mouse_prev.pressed[button] && state.mouse_now.pressed[button];
}

b8 input_mouse_released(MOUSE_BUTTON button) {
    return state.mouse_prev.pressed[button] && !state.mouse_now.pressed[button];
}

void input_get_mouse_position(vec2 pos) {
    pos[0] = (f32)state.mouse_now.x;
    pos[1] = (f32)state.mouse_now.y;
}

void input_get_previous_mouse_position(vec2 pos) {
    pos[0] = (f32)state.mouse_prev.x;
    pos[1] = (f32)state.mouse_prev.y;
}

void input_get_mouse_delta(vec2 delta_pos) {
    delta_pos[0] = (f32)state.mouse_prev.dx;
    delta_pos[1] = (f32)state.mouse_prev.dy;
}

void input_set_mode(platform_window *window, INPUT_MODE mode) {
    if (state.input_mode == mode)
        return;

    state.input_mode = mode;

    state.mouse_now.dx = 0;
    state.mouse_now.dy = 0;

    switch (mode) {
    case INPUT_MODE_UI:
        platform_set_cursor_mode(window, CURSOR_MODE_NORMAL);
        break;
    case INPUT_MODE_GAME:
        platform_set_cursor_mode(window, CURSOR_MODE_LOCKED);
        break;
    }
}