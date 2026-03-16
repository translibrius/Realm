#include "gui/gui_field.h"

#include "gui/gui_text.h"
#include "gui/gui_theme.h"

void gui_field_begin(const char *label, const gui_field_cfg *cfg) {
    f32 label_width = (cfg && cfg->label_width > 0) ? cfg->label_width : 120;
    f32 gap         = (cfg && cfg->gap > 0)         ? cfg->gap         : 8;

    u16 font      = cfg ? cfg->font      : 0;
    u16 font_size = (cfg && cfg->font_size > 0) ? cfg->font_size : 14;

    Clay_Color label_color = gui_theme_get()->text_dim;
    if (cfg && cfg->label_color.a > 0) label_color = cfg->label_color;

    // Outer row
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = (u16)gap,
            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
        },
    });

    // Label column (fixed width so labels align across rows)
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(label_width), .height = CLAY_SIZING_FIT(0)},
        },
    });
    gui_text(label, &(gui_text_cfg){.color = label_color, .size = font_size, .font = font});
    Clay__CloseElement();

    // Caller places control widget(s) here, then calls gui_field_end()
}

void gui_field_end(void) {
    Clay__CloseElement();
}
