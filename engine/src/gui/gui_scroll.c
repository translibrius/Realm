#include "gui/gui_scroll.h"

#include "gui/gui_theme.h"
#include "gui_internal.h"

// Stashed by begin(), read by end(). Single-threaded, non-reentrant (same as Clay).
static Clay_ElementId scroll_eid;
static u32 scroll_base_id;
static gui_scroll_state *scroll_state;
static const gui_scroll_cfg *scroll_cfg;

void gui_scroll_begin(gui_scroll_state *state, const gui_scroll_cfg *cfg) {
    // Auto-generate a stable ID on first use
    if (state && state->_id == 0) {
        state->_id = gui__next_id();
    }

    Clay_ElementId eid = state ? CLAY_IDI("GuiScroll", state->_id) : (Clay_ElementId){0};
    scroll_eid = eid;
    scroll_base_id = state ? state->_id : 0;
    scroll_state = state;
    scroll_cfg = cfg;

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
    // Derive ID from scroll state to keep it stable across frames
    Clay__OpenElementWithId(CLAY_IDI("GuiScrollOuter", scroll_base_id));
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    });

    // Scroll content area — needs explicit ID for Clay_GetScrollContainerData
    // Use cached offset instead of Clay_GetScrollOffset() which matches by
    // layout element pointer — that pointer breaks when dropdown open/close
    // shifts element indices in Clay's ephemeral array.
    Clay_Vector2 child_offset = state ? state->_offset : (Clay_Vector2){0, 0};
    Clay__OpenElementWithId(eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
            .padding = {.left = 8, .right = 4, .top = 4, .bottom = 4},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .childGap = 1,
        },
        .clip = {.vertical = true, .childOffset = child_offset},
    });
    // Caller places children here, then calls gui_scroll_end()
}

void gui_scroll_end(void) {
    // Close the scroll content area
    Clay__CloseElement();

    // Scrollbar
    f32 sb_width = (scroll_cfg && scroll_cfg->scrollbar_width > 0) ? scroll_cfg->scrollbar_width : 8;

    Clay_ScrollContainerData scd = Clay_GetScrollContainerData(scroll_eid);

    // Cache scroll offset for next frame (pointer is valid here since
    // ConfigureOpenElement already updated layoutElement for this element)
    if (scd.found && scroll_state) {
        scroll_state->_offset = *scd.scrollPosition;
    }

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

    const gui_theme *t = gui_theme_get();
    Clay_Color track_color = t->bg_input;
    Clay_Color thumb_color = t->control_hover;
    f32 thumb_radius = 3;
    if (scroll_cfg) {
        if (scroll_cfg->track_color.a > 0) track_color = scroll_cfg->track_color;
        if (scroll_cfg->thumb_color.a > 0) thumb_color = scroll_cfg->thumb_color;
        if (scroll_cfg->thumb_radius > 0) thumb_radius = scroll_cfg->thumb_radius;
    }

    // Scrollbar track — stable ID derived from scroll state
    Clay__OpenElementWithId(CLAY_IDI("GuiScrollTrack", scroll_base_id));
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
        CLAY(CLAY_IDI("GuiScrollSpacer", scroll_base_id), {
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(spacer_frac)}},
        }) {}

        // Thumb
        CLAY(CLAY_IDI("GuiScrollThumb", scroll_base_id), {
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(thumb_h_frac)}},
            .backgroundColor = thumb_color,
            .cornerRadius = CLAY_CORNER_RADIUS(thumb_radius),
        }) {}
    }

    // Close scrollbar track
    Clay__CloseElement();

    // Close outer container
    Clay__CloseElement();

    // Clear stashed state
    scroll_eid = (Clay_ElementId){0};
    scroll_base_id = 0;
    scroll_state = nullptr;
    scroll_cfg = nullptr;
}
