#include "gui/gui_context_menu.h"

#include "gui/gui_button.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "platform/input.h"

void gui_context_menu_open(gui_context_menu_state *state) {
    if (!state) return;
    vec2 mouse;
    input_get_mouse_position(mouse);
    state->pos_x = mouse[0];
    state->pos_y = mouse[1];
    state->open = true;
    state->_just_opened = true;
}

i32 gui_context_menu(gui_context_menu_state *state, const gui_context_menu_cfg *cfg) {
    if (!state || !cfg || !cfg->items || cfg->item_count <= 0) return -1;

    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    const gui_theme *t = gui_theme_get();
    f32 width      = cfg->width > 0      ? cfg->width      : 180;
    u16 font       = cfg->font;
    u16 font_size  = cfg->font_size > 0  ? cfg->font_size  : 13;
    Clay_Color bg    = cfg->color.a > 0      ? cfg->color       : t->bg_elevated;
    Clay_Color hover = cfg->hover_color.a > 0 ? cfg->hover_color : t->control_hover;
    Clay_Color text  = cfg->text_color.a > 0  ? cfg->text_color  : t->text;

    i32 result = -1;

    Clay_ElementId menu_eid = CLAY_IDI("GuiContextMenu", state->_id);
    Clay__OpenElementWithId(menu_eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = state->open ? CLAY_SIZING_FIXED(width) : CLAY_SIZING_FIT(0)},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = state->open ? (Clay_Padding){.top = 4, .bottom = 4} : (Clay_Padding){0},
        },
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .offset = {state->pos_x, state->pos_y},
            .zIndex = 250,
            .pointerCaptureMode = state->open ? CLAY_POINTER_CAPTURE_MODE_CAPTURE
                                              : CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        },
        .backgroundColor = state->open ? bg : (Clay_Color){0},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = state->open ? (Clay_BorderElementConfig){.color = t->border, .width = {1, 1, 1, 1, 0}}
                              : (Clay_BorderElementConfig){0},
    });

    if (state->open) {
        for (i32 i = 0; i < cfg->item_count; i++) {
            gui_button_state btn = gui_button_begin(&(gui_button_cfg){
                .color       = bg,
                .hover_color = hover,
                .press_color = hover,
                .padding     = 6,
                .grow_width  = true,
            });
            gui_text(cfg->items[i], &(gui_text_cfg){.color = text, .size = font_size, .font = font});
            gui_button_end();

            if (btn.clicked) {
                result = i;
                state->open = false;
            }
        }

        // Close on click outside (skip the frame the menu was opened)
        if (state->_just_opened) {
            state->_just_opened = false;
        } else if (input_mouse_pressed(MOUSE_LEFT) || input_mouse_pressed(MOUSE_RIGHT)) {
            Clay_ElementData data = Clay_GetElementData(menu_eid);
            if (data.found) {
                vec2 mouse;
                input_get_mouse_position(mouse);
                Clay_BoundingBox bb = data.boundingBox;
                if (mouse[0] < bb.x || mouse[0] > bb.x + bb.width ||
                    mouse[1] < bb.y || mouse[1] > bb.y + bb.height) {
                    state->open = false;
                }
            } else {
                state->open = false;
            }
        }
    }

    Clay__CloseElement();
    return result;
}
