#include "gui/gui_scroll.h"

#include "gui/gui_theme.h"
#include "gui_internal.h"
#include "platform/input.h"

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

    // Handle ongoing scrollbar drag (before layout so position is updated this frame)
    if (state && state->_dragging) {
        if (!input_is_mouse_down(MOUSE_LEFT)) {
            state->_dragging = false;
        } else {
            Clay_ScrollContainerData scd = Clay_GetScrollContainerData(eid);
            if (scd.found) {
                f32 content_h = scd.contentDimensions.height;
                f32 view_h = scd.scrollContainerDimensions.height;
                f32 max_scroll = content_h - view_h;

                // Get track element bounds for computing scroll ratio
                Clay_ElementId track_eid = CLAY_IDI("GuiScrollTrack", state->_id);
                Clay_ElementData track_data = Clay_GetElementData(track_eid);
                if (track_data.found && max_scroll > 0) {
                    f32 track_h = track_data.boundingBox.height - 4; // padding
                    f32 thumb_h_frac = view_h / content_h;
                    if (thumb_h_frac < 0.05f) thumb_h_frac = 0.05f;
                    if (thumb_h_frac > 1.0f) thumb_h_frac = 1.0f;
                    f32 scroll_range = track_h * (1.0f - thumb_h_frac);

                    if (scroll_range > 0) {
                        vec2 mouse;
                        input_get_mouse_position(mouse);
                        f32 dy = mouse[1] - state->_drag_start_y;
                        f32 scroll_delta = (dy / scroll_range) * max_scroll;
                        f32 new_scroll = state->_drag_start_scroll - scroll_delta;
                        if (new_scroll > 0) new_scroll = 0;
                        if (new_scroll < -max_scroll) new_scroll = -max_scroll;
                        *scd.scrollPosition = (Clay_Vector2){0, new_scroll};
                        state->_offset = *scd.scrollPosition;
                    }
                }
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
    b8 track_hovered = Clay_Hovered();
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

    // Scrollbar click-to-drag interaction (using track bounds from previous frame)
    if (scroll_state && !scroll_state->_dragging && show_scrollbar && scd.found && track_hovered) {
        if (input_mouse_pressed(MOUSE_LEFT)) {
            f32 content_h = scd.contentDimensions.height;
            f32 view_h = scd.scrollContainerDimensions.height;
            f32 max_scroll = content_h - view_h;

            Clay_ElementId track_eid = CLAY_IDI("GuiScrollTrack", scroll_base_id);
            Clay_ElementData track_data = Clay_GetElementData(track_eid);

            if (track_data.found && max_scroll > 0) {
                f32 track_y = track_data.boundingBox.y + 2;
                f32 track_h = track_data.boundingBox.height - 4;

                vec2 mouse;
                input_get_mouse_position(mouse);

                // Jump scroll position to center thumb at click location
                f32 click_frac = (mouse[1] - track_y) / track_h;
                if (click_frac < 0) click_frac = 0;
                if (click_frac > 1) click_frac = 1;

                f32 effective_frac = thumb_h_frac;
                f32 scroll_frac_new = (click_frac - effective_frac * 0.5f) / (1.0f - effective_frac);
                if (scroll_frac_new < 0) scroll_frac_new = 0;
                if (scroll_frac_new > 1) scroll_frac_new = 1;

                f32 new_scroll = -scroll_frac_new * max_scroll;
                *scd.scrollPosition = (Clay_Vector2){0, new_scroll};
                scroll_state->_offset = *scd.scrollPosition;

                scroll_state->_dragging = true;
                scroll_state->_drag_start_y = mouse[1];
                scroll_state->_drag_start_scroll = new_scroll;
                scroll_state->auto_scroll = false;
            }
        }
    }

    // Close outer container
    Clay__CloseElement();

    // Clear stashed state
    scroll_eid = (Clay_ElementId){0};
    scroll_base_id = 0;
    scroll_state = nullptr;
    scroll_cfg = nullptr;
}
