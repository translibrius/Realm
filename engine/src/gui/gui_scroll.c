#include "gui/gui_scroll.h"

#include <string.h>

static Clay_ElementId gui__make_id(const char *id) {
    Clay_String s = {.length = (i32)strlen(id), .chars = id};
    return Clay__HashString(s, 0);
}

void gui_scroll_begin(const char *id, gui_scroll_state *state, const gui_scroll_cfg *cfg) {
    (void)cfg;

    Clay_ElementId eid = gui__make_id(id);

    // Auto-scroll: pin Clay's scroll position to the bottom before layout
    if (state && state->auto_scroll) {
        Clay_ScrollContainerData scd = Clay_GetScrollContainerData(eid);
        if (scd.found) {
            f32 max_scroll = scd.contentDimensions.height - scd.scrollContainerDimensions.height;
            if (max_scroll > 0) {
                *scd.scrollPosition = (Clay_Vector2){0, -max_scroll};
            }
        }
    }

    // Re-enable auto-scroll when user scrolls back to the bottom
    if (state && !state->auto_scroll) {
        Clay_ScrollContainerData scd = Clay_GetScrollContainerData(eid);
        if (scd.found) {
            f32 max_scroll = scd.contentDimensions.height - scd.scrollContainerDimensions.height;
            if (max_scroll > 0 && (max_scroll + scd.scrollPosition->y) < 2.0f) {
                state->auto_scroll = true;
            }
        }
    }

    // Outer container: horizontal row with scroll area + optional scrollbar
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    });

    // Scroll content area
    Clay__OpenElementWithId(eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .padding = {.left = 8, .right = 4, .top = 4, .bottom = 4},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .childGap = 1,
        },
        .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
    });
    // Caller places children here, then calls gui_scroll_end()
}

void gui_scroll_end(const char *id, gui_scroll_state *state, const gui_scroll_cfg *cfg) {
    (void)state;

    // Close the scroll content area
    Clay__CloseElement();

    // Scrollbar
    f32 sb_width = (cfg && cfg->scrollbar_width > 0) ? cfg->scrollbar_width : 8;

    Clay_ElementId eid = gui__make_id(id);
    Clay_ScrollContainerData scd = Clay_GetScrollContainerData(eid);

    b8 show_scrollbar = false;
    f32 thumb_h_frac = 0;
    f32 spacer_frac = 0;

    if (scd.found && scd.contentDimensions.height > scd.scrollContainerDimensions.height) {
        show_scrollbar = true;
        f32 content_h = scd.contentDimensions.height;
        f32 view_h = scd.scrollContainerDimensions.height;
        thumb_h_frac = view_h / content_h;
        if (thumb_h_frac < 0.05f) thumb_h_frac = 0.05f;
        if (thumb_h_frac > 1.0f) thumb_h_frac = 1.0f;
        f32 max_scroll = content_h - view_h;
        f32 scroll_frac = max_scroll > 0 ? -scd.scrollPosition->y / max_scroll : 0;
        if (scroll_frac < 0) scroll_frac = 0;
        if (scroll_frac > 1) scroll_frac = 1;
        spacer_frac = scroll_frac * (1.0f - thumb_h_frac);
    }

    Clay_Color track_color = {25, 25, 28, 255};
    Clay_Color thumb_color = {80, 80, 90, 180};
    f32 thumb_radius = 3;
    if (cfg) {
        if (cfg->track_color.a > 0) track_color = cfg->track_color;
        if (cfg->thumb_color.a > 0) thumb_color = cfg->thumb_color;
        if (cfg->thumb_radius > 0) thumb_radius = cfg->thumb_radius;
    }

    // Scrollbar track
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(sb_width), .height = CLAY_SIZING_GROW(0)},
            .padding = {.top = 2, .bottom = 2},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = track_color,
    });

    if (show_scrollbar) {
        // Spacer above thumb
        Clay__OpenElement();
        Clay__ConfigureOpenElement((Clay_ElementDeclaration){
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(spacer_frac)}},
        });
        Clay__CloseElement();

        // Thumb
        Clay__OpenElement();
        Clay__ConfigureOpenElement((Clay_ElementDeclaration){
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(thumb_h_frac)}},
            .backgroundColor = thumb_color,
            .cornerRadius = CLAY_CORNER_RADIUS(thumb_radius),
        });
        Clay__CloseElement();
    }

    // Close scrollbar track
    Clay__CloseElement();

    // Close outer container
    Clay__CloseElement();
}
