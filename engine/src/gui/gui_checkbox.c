#include "gui/gui_checkbox.h"

#include "gui/gui_button.h"
#include "gui/gui_theme.h"

b8 gui_checkbox(b8 *checked, const gui_checkbox_cfg *cfg) {
    if (!checked) return false;

    f32 size   = (cfg && cfg->size > 0)           ? cfg->size          : 18;
    f32 radius = (cfg && cfg->corner_radius > 0)   ? cfg->corner_radius : 3;

    const gui_theme *t = gui_theme_get();
    Clay_Color color_unchecked = t->control;
    Clay_Color color_hover     = t->control_hover;
    Clay_Color color_checked   = t->accent;
    if (cfg) {
        if (cfg->color.a > 0)         color_unchecked = cfg->color;
        if (cfg->hover_color.a > 0)   color_hover     = cfg->hover_color;
        if (cfg->checked_color.a > 0) color_checked   = cfg->checked_color;
    }

    Clay_Color bg = *checked ? color_checked : color_unchecked;
    Clay_Color hover = *checked ? t->accent_hover : color_hover;

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
