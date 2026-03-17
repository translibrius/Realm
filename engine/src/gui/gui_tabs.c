#include "gui/gui_tabs.h"

#include "gui/gui_button.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"

i32 gui_tabs(i32 *selected, const char **labels, i32 count, const gui_tabs_cfg *cfg) {
    if (!selected || !labels || count <= 0) return 0;

    f32 padding   = (cfg && cfg->padding > 0)       ? cfg->padding       : 8;
    f32 gap       = (cfg && cfg->gap > 0)            ? cfg->gap           : 2;
    f32 radius    = (cfg && cfg->corner_radius > 0)  ? cfg->corner_radius : 4;
    u16 font      = cfg ? cfg->font : 0;
    u16 font_size = (cfg && cfg->font_size > 0)      ? cfg->font_size     : 14;

    const gui_theme *t = gui_theme_get();
    Clay_Color inactive_color = t->bg_titlebar;
    Clay_Color active_color   = t->bg_secondary;
    Clay_Color hover_color    = t->control_hover;
    Clay_Color text_color     = t->text;
    if (cfg) {
        if (cfg->color.a > 0)        inactive_color = cfg->color;
        if (cfg->active_color.a > 0) active_color   = cfg->active_color;
        if (cfg->hover_color.a > 0)  hover_color    = cfg->hover_color;
        if (cfg->text_color.a > 0)   text_color     = cfg->text_color;
    }

    // Tab strip container
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = (u16)gap,
            .padding = {.left = 4, .right = 4},
        },
        .backgroundColor = inactive_color,
    });

    i32 result = *selected;

    for (i32 i = 0; i < count; i++) {
        b8 is_active = (i == *selected);

        Clay_Color tab_text = is_active ? text_color : (Clay_Color){text_color.r * 0.7f, text_color.g * 0.7f, text_color.b * 0.7f, text_color.a};

        gui_button_state btn = gui_button_begin(&(gui_button_cfg){
            .color        = is_active ? active_color : inactive_color,
            .hover_color  = is_active ? active_color : hover_color,
            .press_color  = active_color,
            .padding      = padding,
            .corner_radius = is_active ? radius : 0,
        });
        gui_text(labels[i], &(gui_text_cfg){.color = tab_text, .size = font_size, .font = font});
        gui_button_end();

        if (btn.clicked && !is_active) {
            result = i;
        }
    }

    Clay__CloseElement();

    *selected = result;
    return result;
}
