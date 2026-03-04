#include "gui/gui_checkbox.h"

#include "gui/gui_button.h"

b8 gui_checkbox(b8 *checked, const gui_checkbox_cfg *cfg) {
    if (!checked) return false;

    f32 size   = (cfg && cfg->size > 0)           ? cfg->size          : 18;
    f32 radius = (cfg && cfg->corner_radius > 0)   ? cfg->corner_radius : 3;

    Clay_Color color_unchecked = {50, 50, 55, 255};
    Clay_Color color_hover     = {65, 65, 70, 255};
    Clay_Color color_checked   = {70, 130, 220, 255};
    if (cfg) {
        if (cfg->color.a > 0)         color_unchecked = cfg->color;
        if (cfg->hover_color.a > 0)   color_hover     = cfg->hover_color;
        if (cfg->checked_color.a > 0) color_checked   = cfg->checked_color;
    }

    Clay_Color bg = *checked ? color_checked : color_unchecked;
    Clay_Color hover = *checked ? (Clay_Color){90, 150, 240, 255} : color_hover;

    gui_button_state btn = gui_button_begin(&(gui_button_cfg){
        .color       = bg,
        .hover_color = hover,
        .press_color = bg,
        .width       = size,
        .height      = size,
        .corner_radius = radius,
    });

    gui_button_end();

    if (btn.clicked) {
        *checked = !*checked;
        return true;
    }
    return false;
}
