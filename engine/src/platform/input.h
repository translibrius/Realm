#pragma once
#include "defines.h"
#include "platform.h"

#include "../vendor/cglm/cglm.h"

typedef enum INPUT_MODE {
    INPUT_MODE_UI, // Uses OS cursor
    INPUT_MODE_GAME, // Hides OS cursor, uses raw input
} INPUT_MODE;

typedef enum MOUSE_BUTTON {
    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,

    MOUSE_MAX_BUTTONS
} MOUSE_BUTTON;

typedef enum KEYBOARD_KEY {
    KEY_BACKSPACE,
    KEY_ENTER,
    KEY_TAB,

    KEY_L_SHIFT, KEY_R_SHIFT,
    KEY_L_CTRL, KEY_R_CTRL,
    KEY_L_ALT, KEY_R_ALT,
    KEY_L_SUPER, KEY_R_SUPER,

    KEY_ESCAPE,

    KEY_SPACE,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    // ABC
    KEY_A, KEY_B, KEY_C, KEY_D,
    KEY_E, KEY_F, KEY_G, KEY_H,
    KEY_I, KEY_J, KEY_K, KEY_L,
    KEY_M, KEY_N, KEY_O, KEY_P,
    KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X,
    KEY_Y, KEY_Z,

    KEY_NUMPAD0,
    KEY_NUMPAD1,
    KEY_NUMPAD2,
    KEY_NUMPAD3,
    KEY_NUMPAD4,
    KEY_NUMPAD5,
    KEY_NUMPAD6,
    KEY_NUMPAD7,
    KEY_NUMPAD8,
    KEY_NUMPAD9,
    KEY_MULTIPLY,
    KEY_ADD,
    KEY_SEPARATOR,
    KEY_SUBTRACT,
    KEY_DECIMAL,
    KEY_DIVIDE,

    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,

    KEY_NUMLOCK,
    KEY_SCROLL,
    KEY_NUMPAD_EQUAL,

    KEY_SEMICOLON,
    KEY_PLUS,
    KEY_COMMA,
    KEY_MINUS,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_GRAVE,

    KEY_MAX_KEYS,
} KEYBOARD_KEY;

typedef struct input_key {
    KEYBOARD_KEY key;
    b8 pressed;
} input_key;

typedef struct input_mouse_button {
    MOUSE_BUTTON button;
    b8 pressed;
} input_mouse_button;

typedef struct input_mouse_move {
    i16 x, y;
} input_mouse_move;

typedef struct input_mouse_scroll {
    i16 z_delta;
} input_mouse_scroll;

void input_system_init();

// Perform mapping from keycode to button per platform
void input_process_key(KEYBOARD_KEY key, b8 is_pressed);

void input_process_mouse_button(MOUSE_BUTTON button, b8 is_pressed);
void input_process_mouse_move(i32 position_x, i32 position_y);
void input_process_mouse_scroll(i32 delta); // Flatten the input to an OS-independent (-1, 1)

REALM_API b8 input_is_key_down(KEYBOARD_KEY key); // now
REALM_API b8 input_key_pressed(KEYBOARD_KEY key); // up -> down
REALM_API b8 input_key_released(KEYBOARD_KEY key); // down -> up

REALM_API b8 input_is_mouse_down(MOUSE_BUTTON button);
REALM_API b8 input_mouse_pressed(MOUSE_BUTTON button);
REALM_API b8 input_mouse_released(MOUSE_BUTTON button);

REALM_API void input_get_mouse_position(vec2 pos);
REALM_API void input_get_previous_mouse_position(vec2 pos);
REALM_API void input_get_mouse_delta(vec2 delta_pos);

void input_update();