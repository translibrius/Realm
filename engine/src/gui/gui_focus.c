#include "gui/gui_focus.h"

#include "platform/input.h"

static u32 s_active_id;

void gui_focus_begin_frame(void) {
    // Any click clears focus; the clicked widget re-claims it in its render pass.
    if (input_mouse_pressed(MOUSE_LEFT) || input_mouse_pressed(MOUSE_RIGHT)) {
        s_active_id = 0;
    }
}

void gui_focus_set(u32 id) {
    s_active_id = id;
}

void gui_focus_clear(void) {
    s_active_id = 0;
}

u32 gui_focus_get(void) {
    return s_active_id;
}

b8 gui_focus_is(u32 id) {
    return id != 0 && s_active_id == id;
}
