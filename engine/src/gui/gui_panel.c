#include "gui/gui_panel.h"

#include "clay.h"
#include "gui/gui_theme.h"

// Per-frame sequential counter for stable Clay element IDs.
// Without this, panels/spacers/separators use Clay auto-IDs that are derived
// from (child_index + floatingChildrenCount) of the parent element.  When a
// sibling widget (e.g. dropdown) conditionally creates a floating element,
// floatingChildrenCount changes and every subsequent auto-ID sibling shifts,
// causing a full-layout ID mismatch and visible flicker.
static u32 gui_panel_counter;

void gui_panel_frame_reset_(void) {
    gui_panel_counter = 0;
}

static Clay_SizingAxis resolve_sizing(gui_sizing mode, f32 value) {
    switch (mode) {
    case GUI_SIZE_GROW:    return CLAY_SIZING_GROW(0);
    case GUI_SIZE_FIXED:   return CLAY_SIZING_FIXED(value);
    case GUI_SIZE_PERCENT: return CLAY_SIZING_PERCENT(value);
    default: // GUI_SIZE_FIT — auto-promote to FIXED if value > 0
        return value > 0 ? CLAY_SIZING_FIXED(value) : CLAY_SIZING_FIT(0);
    }
}

void gui_panel_begin(const gui_panel_cfg *cfg) {
    Clay_SizingAxis w_sizing = CLAY_SIZING_FIT(0);
    Clay_SizingAxis h_sizing = CLAY_SIZING_FIT(0);

    if (cfg) {
        w_sizing = resolve_sizing(cfg->width_sizing, cfg->width);
        h_sizing = resolve_sizing(cfg->height_sizing, cfg->height);
    }

    Clay_ElementDeclaration decl = {0};
    decl.layout.sizing.width = w_sizing;
    decl.layout.sizing.height = h_sizing;
    decl.layout.layoutDirection = (cfg && cfg->horizontal) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;

    if (cfg) {
        decl.layout.padding = CLAY_PADDING_ALL((u16)cfg->padding);
        decl.layout.childGap = (u16)cfg->gap;
        decl.layout.childAlignment.x = cfg->align_x;
        decl.layout.childAlignment.y = cfg->align_y;
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

    gui_panel_counter++;
    Clay__OpenElementWithId(CLAY_IDI("GuiPanel", gui_panel_counter));
    Clay__ConfigureOpenElement(decl);
}

void gui_panel_end(void) {
    Clay__CloseElement();
}

void gui_row(f32 gap) {
    gui_panel_begin(&(gui_panel_cfg){.horizontal = true, .gap = gap, .width_sizing = GUI_SIZE_GROW});
}

void gui_row_end(void) { gui_panel_end(); }

void gui_col(f32 gap) {
    gui_panel_begin(&(gui_panel_cfg){.gap = gap});
}

void gui_col_end(void) { gui_panel_end(); }

void gui_spacer(void) {
    gui_panel_counter++;
    CLAY(CLAY_IDI("GuiPanel", gui_panel_counter), {
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
    }) {}
}

void gui_spacer_fixed(f32 size) {
    gui_panel_counter++;
    CLAY(CLAY_IDI("GuiPanel", gui_panel_counter), {
        .layout = {.sizing = {.width = CLAY_SIZING_FIXED(size), .height = CLAY_SIZING_FIXED(size)}},
    }) {}
}

void gui_separator(void) {
    gui_panel_counter++;
    CLAY(CLAY_IDI("GuiPanel", gui_panel_counter), {
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(1)}},
        .backgroundColor = gui_theme_get()->separator,
    }) {}
}
