#include "gui/gui_dropdown.h"

#include "gui/gui_button.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"
#include "platform/platform.h"

b8 gui_dropdown(gui_dropdown_state *state, const gui_dropdown_cfg *cfg) {
    if (!state || !cfg || !cfg->items || cfg->item_count <= 0) return false;

    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    f32 width      = cfg->width > 0       ? cfg->width       : 200;
    f32 radius     = cfg->corner_radius;
    u16 font       = cfg->font;
    u16 font_size  = cfg->font_size > 0   ? cfg->font_size   : 14;

    const gui_theme *t = gui_theme_get();
    Clay_Color bg_color    = t->control;
    Clay_Color list_color  = t->bg_elevated;
    Clay_Color hover_color = t->control_hover;
    Clay_Color text_color  = t->text;
    if (cfg->color.a > 0)      bg_color    = cfg->color;
    if (cfg->list_color.a > 0) list_color  = cfg->list_color;
    if (cfg->hover_color.a > 0) hover_color = cfg->hover_color;
    if (cfg->text_color.a > 0) text_color  = cfg->text_color;

    i32 old_selected = state->selected;

    // Current value display — acts like a button
    const char *display = cfg->label ? cfg->label
                        : (state->selected >= 0 && state->selected < cfg->item_count)
                              ? cfg->items[state->selected]
                              : "---";

    // Wrapper with explicit ID so the floating list can attach to it
    Clay_ElementId trigger_eid = CLAY_IDI("GuiDropdown", state->_id);
    Clay__OpenElementWithId(trigger_eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {.width = CLAY_SIZING_FIT(0)}},
    });

    // Label mode (menu bar): fit-to-content trigger. Normal mode: fixed width.
    gui_button_state btn = gui_button_begin(&(gui_button_cfg){
        .color        = bg_color,
        .hover_color  = hover_color,
        .press_color  = bg_color,
        .width        = cfg->label ? 0 : width,
        .padding      = cfg->label ? 8 : 8,
        .corner_radius = radius,
        .corners      = cfg->trigger_corners,
    });
    gui_text(display, &(gui_text_cfg){.color = text_color, .size = font_size, .font = font});
    gui_button_end();

    Clay__CloseElement(); // wrapper

    state->_trigger_hovered = btn.hovered;

    if (btn.clicked) {
        state->open = !state->open;
    }

    // Decide whether to open upward or downward.
    // Use actual list height from previous frame if available, otherwise estimate.
    Clay_ElementId list_eid = CLAY_IDI("GuiDropdownList", state->_id);
    b8 open_upward = false;
    {
        Clay_ElementData trigger_data = Clay_GetElementData(trigger_eid);
        if (trigger_data.found) {
            Clay_ElementData list_data = Clay_GetElementData(list_eid);
            f32 list_h = list_data.found ? list_data.boundingBox.height : 0;
            f32 trigger_bottom = trigger_data.boundingBox.y + trigger_data.boundingBox.height;
            f32 trigger_top = trigger_data.boundingBox.y;
            platform_window *win = renderer_get_active_window();
            f32 win_h = win ? (f32)win->settings.height : 800.0f;
            f32 space_below = win_h - trigger_bottom;
            f32 space_above = trigger_top;
            if (list_h > space_below && space_above > space_below) {
                open_upward = true;
            }
        }
    }

    // Floating dropdown list — always created to keep parent's
    // floatingChildrenCount constant and prevent auto-ID shifts.
    Clay__OpenElementWithId(list_eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = state->open
                           ? (cfg->label ? CLAY_SIZING_FIT(cfg->min_width) : CLAY_SIZING_FIXED(width))
                           : CLAY_SIZING_FIT(0)},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = state->open ? (Clay_Padding){.top = 2, .bottom = 2} : (Clay_Padding){0},
        },
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
            .parentId = trigger_eid.id,
            .attachPoints = open_upward
                ? (Clay_FloatingAttachPoints){
                    .element = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                    .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                  }
                : (Clay_FloatingAttachPoints){
                    .element = CLAY_ATTACH_POINT_LEFT_TOP,
                    .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                  },
            .zIndex = 200,
            .pointerCaptureMode = state->open ? CLAY_POINTER_CAPTURE_MODE_CAPTURE
                                              : CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        },
        .backgroundColor = state->open ? list_color : (Clay_Color){0},
        .cornerRadius = CLAY_CORNER_RADIUS(radius),
        .border = state->open ? (Clay_BorderElementConfig){.color = t->border, .width = {1, 1, 1, 1, 0}}
                              : (Clay_BorderElementConfig){0},
    });

    if (state->open) {
        for (i32 i = 0; i < cfg->item_count; i++) {
            gui_button_state item_btn = gui_button_begin(&(gui_button_cfg){
                .color       = list_color,
                .hover_color = hover_color,
                .press_color = hover_color,
                .padding     = 6,
                .grow_width  = true,
            });
            gui_text(cfg->items[i], &(gui_text_cfg){.color = text_color, .size = font_size, .font = font});
            gui_button_end();

            if (item_btn.clicked) {
                state->selected = i;
                state->open = false;
            }
        }

        // Close on click outside — if mouse pressed and not hovering the list
        if (input_mouse_pressed(MOUSE_LEFT) && !btn.hovered) {
            Clay_ElementData list_data = Clay_GetElementData(list_eid);
            if (list_data.found) {
                vec2 mouse;
                input_get_mouse_position(mouse);
                Clay_BoundingBox bb = list_data.boundingBox;
                if (mouse[0] < bb.x || mouse[0] > bb.x + bb.width ||
                    mouse[1] < bb.y || mouse[1] > bb.y + bb.height) {
                    state->open = false;
                }
            } else {
                state->open = false;
            }
        }
    }

    Clay__CloseElement(); // list

    return state->selected != old_selected;
}
