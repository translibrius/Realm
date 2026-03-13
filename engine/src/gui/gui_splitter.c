#include "gui/gui_splitter.h"

#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "platform/input.h"

static b8 gui_splitter_impl(gui_splitter_state *state, f32 *target, const gui_splitter_cfg *cfg, b8 vertical) {
    if (!state || !target) return false;

    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    f32 thickness = (cfg && cfg->thickness > 0)  ? cfg->thickness : 5;
    f32 min_val   = (cfg && cfg->min_value > 0)  ? cfg->min_value : 50;
    f32 max_val   = (cfg && cfg->max_value > 0)  ? cfg->max_value : 800;
    b8  invert    = cfg ? cfg->invert : false;

    const gui_theme *t = gui_theme_get();
    Clay_Color color       = t->separator;
    Clay_Color hover_color = t->accent;
    if (cfg) {
        if (cfg->color.a > 0)       color       = cfg->color;
        if (cfg->hover_color.a > 0) hover_color = cfg->hover_color;
    }

    f32 old_value = *target;

    Clay_ElementId eid = CLAY_IDI("GuiSplitter", state->_id);
    Clay__OpenElementWithId(eid);
    b8 hovered = Clay_Hovered();

    // Vertical splitter: fixed width, grow height. Horizontal: grow width, fixed height.
    Clay_SizingAxis w = vertical ? CLAY_SIZING_FIXED(thickness) : CLAY_SIZING_GROW(0);
    Clay_SizingAxis h = vertical ? CLAY_SIZING_GROW(0)          : CLAY_SIZING_FIXED(thickness);

    Clay_Color bg = (hovered || state->dragging) ? hover_color : color;

    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {.width = w, .height = h}},
        .backgroundColor = bg,
    });

    Clay__CloseElement();

    // Drag logic
    if (hovered && input_mouse_pressed(MOUSE_LEFT)) {
        state->dragging = true;
    }
    if (state->dragging && !input_is_mouse_down(MOUSE_LEFT)) {
        state->dragging = false;
    }

    if (state->dragging) {
        vec2 delta;
        input_get_mouse_delta(delta);
        f32 d = vertical ? delta[0] : delta[1];
        if (invert) d = -d;
        *target += d;
        if (*target < min_val) *target = min_val;
        if (*target > max_val) *target = max_val;
    }

    return *target != old_value;
}

b8 gui_splitter_v(gui_splitter_state *state, f32 *target, const gui_splitter_cfg *cfg) {
    return gui_splitter_impl(state, target, cfg, true);
}

b8 gui_splitter_h(gui_splitter_state *state, f32 *target, const gui_splitter_cfg *cfg) {
    return gui_splitter_impl(state, target, cfg, false);
}
