#include "gui/gui_checkbox.h"

#include "gui/gui_button.h"

b8 gui_checkbox(b8 *checked, const gui_checkbox_cfg *cfg) {
    if (!checked) return false;

    f32 size   = (cfg && cfg->size > 0)           ? cfg->size          : 18;
    f32 radius = (cfg && cfg->corner_radius > 0)   ? cfg->corner_radius : 3;

    Clay_Color color_unchecked = {50, 50, 55, 255};
    Clay_Color color_hover     = {65, 65, 70, 255};
    Clay_Color color_checked   = {70, 130, 220, 255};
    Clay_Color color_check     = {230, 230, 235, 255};
    if (cfg) {
        if (cfg->color.a > 0)         color_unchecked = cfg->color;
        if (cfg->hover_color.a > 0)   color_hover     = cfg->hover_color;
        if (cfg->checked_color.a > 0) color_checked   = cfg->checked_color;
        if (cfg->check_color.a > 0)   color_check     = cfg->check_color;
    }

    Clay_Color bg = *checked ? color_checked : color_unchecked;

    gui_button_state btn = gui_button_begin(&(gui_button_cfg){
        .color       = bg,
        .hover_color = *checked ? color_checked : color_hover,
        .press_color = bg,
        .width       = size,
        .height      = size,
        .corner_radius = radius,
    });

    // Inner check mark (small filled square)
    if (*checked) {
        f32 inner = size * 0.5f;
        CLAY_AUTO_ID({
            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(inner), .height = CLAY_SIZING_FIXED(inner)}},
            .backgroundColor = color_check,
            .cornerRadius = CLAY_CORNER_RADIUS(radius > 1 ? radius - 1 : 0),
        }) {}
    }

    gui_button_end();

    if (btn.clicked) {
        *checked = !*checked;
        return true;
    }
    return false;
}
