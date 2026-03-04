#include "gui/gui_panel.h"

#include <string.h>

// Helper: build Clay_ElementId from a runtime string.
static Clay_ElementId gui__make_id(const char *id) {
    Clay_String s = {.length = (i32)strlen(id), .chars = id};
    return Clay__HashString(s, 0);
}

void gui_panel_begin(const char *id, const gui_panel_cfg *cfg) {
    Clay_ElementId eid = gui__make_id(id);

    Clay_SizingAxis w_sizing = CLAY_SIZING_FIT(0);
    Clay_SizingAxis h_sizing = CLAY_SIZING_FIT(0);

    if (cfg) {
        if (cfg->grow_width) {
            w_sizing = CLAY_SIZING_GROW(0);
        } else if (cfg->width > 0) {
            w_sizing = CLAY_SIZING_FIXED(cfg->width);
        }
        if (cfg->grow_height) {
            h_sizing = CLAY_SIZING_GROW(0);
        } else if (cfg->height > 0) {
            h_sizing = CLAY_SIZING_FIXED(cfg->height);
        }
    }

    Clay_ElementDeclaration decl = {0};
    decl.layout.sizing.width = w_sizing;
    decl.layout.sizing.height = h_sizing;
    decl.layout.layoutDirection = (cfg && cfg->horizontal) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;

    if (cfg) {
        decl.layout.padding = CLAY_PADDING_ALL((u16)cfg->padding);
        decl.layout.childGap = (u16)cfg->gap;
        decl.backgroundColor = cfg->color;
        if (cfg->corner_radius > 0) {
            decl.cornerRadius = CLAY_CORNER_RADIUS(cfg->corner_radius);
        }
        if (cfg->border_width > 0) {
            decl.border.color = cfg->border_color;
            decl.border.width = (Clay_BorderWidth){
                (u16)cfg->border_width, (u16)cfg->border_width,
                (u16)cfg->border_width, (u16)cfg->border_width, 0,
            };
        }
    }

    Clay__OpenElementWithId(eid);
    Clay__ConfigureOpenElement(decl);
}

void gui_panel_end(void) {
    Clay__CloseElement();
}

void gui_spacer(void) {
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
    });
    Clay__CloseElement();
}

void gui_spacer_fixed(f32 size) {
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(size), .height = CLAY_SIZING_FIXED(size)}},
    });
    Clay__CloseElement();
}
