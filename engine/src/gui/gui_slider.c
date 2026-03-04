#include "gui/gui_slider.h"

#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "platform/input.h"

b8 gui_slider(gui_slider_state *state, const gui_slider_cfg *cfg) {
    if (!state) return false;

    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    f32 width  = (cfg && cfg->width > 0)  ? cfg->width  : 200;
    f32 height = (cfg && cfg->height > 0) ? cfg->height : 16;
    f32 radius = (cfg && cfg->corner_radius > 0) ? cfg->corner_radius : height * 0.5f;

    const gui_theme *t = gui_theme_get();
    Clay_Color track_color = t->control;
    Clay_Color fill_color  = t->accent;
    Clay_Color thumb_color = t->control_thumb;
    if (cfg) {
        if (cfg->track_color.a > 0) track_color = cfg->track_color;
        if (cfg->fill_color.a > 0)  fill_color  = cfg->fill_color;
        if (cfg->thumb_color.a > 0) thumb_color = cfg->thumb_color;
    }

    // Clamp value
    if (state->value < 0) state->value = 0;
    if (state->value > 1) state->value = 1;

    f32 old_value = state->value;
    f32 thumb_size = height;

    // Get bounding box from previous frame for interaction
    Clay_ElementId eid = CLAY_IDI("GuiSlider", state->_id);
    Clay_ElementData ed = Clay_GetElementData(eid);

    // Interaction: start drag on click, update value while dragging
    b8 hovered = false;

    Clay__OpenElementWithId(eid);
    hovered = Clay_Hovered();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = track_color,
        .cornerRadius = CLAY_CORNER_RADIUS(radius),
    });

    if (hovered && input_mouse_pressed(MOUSE_LEFT)) {
        state->dragging = true;
    }
    if (state->dragging && !input_is_mouse_down(MOUSE_LEFT)) {
        state->dragging = false;
    }

    // Calculate value from mouse position
    if (state->dragging && ed.found) {
        vec2 mouse;
        input_get_mouse_position(mouse);
        f32 rel = mouse[0] - ed.boundingBox.x;
        state->value = rel / ed.boundingBox.width;
        if (state->value < 0) state->value = 0;
        if (state->value > 1) state->value = 1;
    }

    // Fill portion
    f32 fill_width = state->value * (width - thumb_size);
    if (fill_width < 0) fill_width = 0;

    CLAY_AUTO_ID({
        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(fill_width), .height = CLAY_SIZING_GROW(0)}},
        .backgroundColor = fill_color,
        .cornerRadius = {radius, 0, radius, 0},
    }) {}

    // Thumb
    CLAY_AUTO_ID({
        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(thumb_size), .height = CLAY_SIZING_FIXED(thumb_size)}},
        .backgroundColor = thumb_color,
        .cornerRadius = CLAY_CORNER_RADIUS(thumb_size * 0.5f),
    }) {}

    Clay__CloseElement();

    return state->value != old_value;
}
