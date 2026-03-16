#include "gui/gui_input.h"

#include "core/event.h"
#include "gui/gui_focus.h"
#include "gui/gui_number_input.h"
#include "gui/gui_text_input.h"
#include "platform/input.h"

static b8 gui_input_on_key(void *event, void *user_data) {
    (void)user_data;

    gui_input_type type = gui_focus_input_type();
    if (type == GUI_INPUT_NONE) return false;

    input_key *k = event;
    if (!k || !k->pressed) return false;

    switch (type) {
    case GUI_INPUT_TEXT: {
        gui_text_input_state *s = gui_focus_input_state();
        if (!s) return false;
        if (gui_text_input_handle_key(s, k)) {
            s->submitted = true;
        }
        return true;
    }
    case GUI_INPUT_NUMBER: {
        gui_number_input_state *s = gui_focus_input_state();
        if (!s) return false;
        gui_number_input_handle_key(s, k);
        return true;
    }
    default:
        return false;
    }
}

static b8 gui_input_on_char(void *event, void *user_data) {
    (void)user_data;

    gui_input_type type = gui_focus_input_type();
    if (type == GUI_INPUT_NONE) return false;

    input_char *ch = event;
    if (!ch) return false;

    switch (type) {
    case GUI_INPUT_TEXT: {
        gui_text_input_state *s = gui_focus_input_state();
        if (!s) return false;
        gui_text_input_handle_char(s, ch);
        return true;
    }
    case GUI_INPUT_NUMBER: {
        gui_number_input_state *s = gui_focus_input_state();
        if (!s) return false;
        gui_number_input_handle_char(s, ch);
        return true;
    }
    default:
        return false;
    }
}

void gui_input_init(void) {
    event_register(EVENT_KEY_PRESS, gui_input_on_key, nullptr);
    event_register(EVENT_CHAR_INPUT, gui_input_on_char, nullptr);
}
