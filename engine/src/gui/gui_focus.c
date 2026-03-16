#include "gui/gui_focus.h"

#include "platform/input.h"

static u32 s_active_id;
static gui_input_type s_input_type;
static void *s_input_state;

void gui_focus_begin_frame(void) {
    // Any click clears focus; the clicked widget re-claims it in its render pass.
    if (input_mouse_pressed(MOUSE_LEFT) || input_mouse_pressed(MOUSE_RIGHT)) {
        s_active_id = 0;
        s_input_type = GUI_INPUT_NONE;
        s_input_state = nullptr;
    }
}

void gui_focus_set(u32 id) {
    s_active_id = id;
    s_input_type = GUI_INPUT_NONE;
    s_input_state = nullptr;
}

void gui_focus_set_input(u32 id, gui_input_type type, void *state) {
    s_active_id = id;
    s_input_type = type;
    s_input_state = state;
}

void gui_focus_clear(void) {
    s_active_id = 0;
    s_input_type = GUI_INPUT_NONE;
    s_input_state = nullptr;
}

u32 gui_focus_get(void) {
    return s_active_id;
}

b8 gui_focus_is(u32 id) {
    return id != 0 && s_active_id == id;
}

gui_input_type gui_focus_input_type(void) {
    return s_input_type;
}

void *gui_focus_input_state(void) {
    return s_input_state;
}
