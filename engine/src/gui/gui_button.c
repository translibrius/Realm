#include "gui/gui_button.h"

#include "platform/input.h"

#include <string.h>

static Clay_ElementId gui__make_id(const char *id) {
    Clay_String s = {.length = (i32)strlen(id), .chars = id};
    return Clay__HashString(s, 0);
}

// Only one element can be "pressed" at a time (single mouse button).
static u32 gui_button_pressed_id;

gui_button_state gui_button_begin(const char *id, const gui_button_cfg *cfg) {
    Clay_ElementId eid = gui__make_id(id);

    gui_button_state result = {0};
    result.hovered = Clay_PointerOver(eid);

    if (input_mouse_pressed(MOUSE_LEFT) && result.hovered) {
        gui_button_pressed_id = eid.id;
    }

    result.pressed = result.hovered && gui_button_pressed_id == eid.id;

    if (gui_button_pressed_id == eid.id && !input_is_mouse_down(MOUSE_LEFT)) {
        gui_button_pressed_id = 0;
        if (result.hovered) {
            result.clicked = true;
        }
    }

    // Build visual element
    Clay_Color bg = {0};
    if (cfg) {
        bg = result.pressed ? cfg->press_color
           : result.hovered ? cfg->hover_color
                            : cfg->color;
    }

    Clay_SizingAxis w_sizing = CLAY_SIZING_FIT(0);
    Clay_SizingAxis h_sizing = CLAY_SIZING_FIT(0);
    if (cfg) {
        if (cfg->width > 0) w_sizing = CLAY_SIZING_FIXED(cfg->width);
        if (cfg->height > 0) h_sizing = CLAY_SIZING_FIXED(cfg->height);
    }

    Clay_ElementDeclaration decl = {
        .layout = {
            .sizing = {.width = w_sizing, .height = h_sizing},
            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = bg,
    };
    if (cfg) {
        decl.layout.padding = CLAY_PADDING_ALL((u16)cfg->padding);
        if (cfg->corner_radius > 0) {
            decl.cornerRadius = CLAY_CORNER_RADIUS(cfg->corner_radius);
        }
    }

    Clay__OpenElementWithId(eid);
    Clay__ConfigureOpenElement(decl);

    return result;
}

void gui_button_end(void) {
    Clay__CloseElement();
}
