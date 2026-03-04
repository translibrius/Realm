#include "gui/gui_window.h"

#include "gui/gui_button.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui_internal.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

gui_window_result gui_window_begin(gui_window_state *state, const gui_window_cfg *cfg) {
    gui_window_result result = {0};

    if (!state || !state->visible || !cfg) {
        return result;
    }

    result.visible = true;

    // Auto-generate a stable ID on first use
    if (state->_id == 0) {
        state->_id = gui__next_id();
    }

    f32 corner_radius = cfg->corner_radius > 0 ? cfg->corner_radius : 6;
    u16 font_size = cfg->font_size > 0 ? cfg->font_size : 13;
    i32 z_index = cfg->z_index > 0 ? cfg->z_index : 100;

    // Window bounds clamping
    platform_window *win = renderer_get_active_window();
    f32 win_w = win ? (f32)win->settings.width : 700.0f;
    f32 win_h = win ? (f32)win->settings.height : 400.0f;

    // Apply ongoing drag delta (before layout so position is up-to-date)
    b8 mouse_down = input_is_mouse_down(MOUSE_LEFT);
    if (state->dragging) {
        if (mouse_down) {
            vec2 delta;
            input_get_mouse_delta(delta);
            state->pos_x += delta[0];
            state->pos_y += delta[1];
        } else {
            state->dragging = false;
        }
    }

    // Clamp to window bounds
    f32 max_x = (win_w - cfg->width) * 0.5f;
    if (max_x < 0) max_x = 0;
    if (state->pos_x < -max_x) state->pos_x = -max_x;
    if (state->pos_x > max_x) state->pos_x = max_x;
    if (state->pos_y < 0) state->pos_y = 0;
    if (state->pos_y + cfg->height > win_h) state->pos_y = win_h - cfg->height;

    // === Outer panel (floating) — needs explicit ID for floating attachment ===
    Clay_ElementId panel_eid = CLAY_IDI("GuiWindow", state->_id);
    Clay__OpenElementWithId(panel_eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(cfg->width), .height = CLAY_SIZING_FIXED(cfg->height)},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .offset = {state->pos_x, state->pos_y},
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_CENTER_TOP,
                .parent = CLAY_ATTACH_POINT_CENTER_TOP,
            },
            .zIndex = (i16)z_index,
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
        },
        .backgroundColor = cfg->bg_color,
        .cornerRadius = CLAY_CORNER_RADIUS(corner_radius),
        .border = {.color = cfg->border_color, .width = {1, 1, 1, 1, 0}},
    });

    // === Header ===
    Clay__OpenElement();
    b8 header_hovered = Clay_Hovered();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)},
            .padding = {.left = 10, .right = 6},
            .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = cfg->header_color,
        .cornerRadius = {corner_radius, corner_radius, 0, 0},
        .border = {.color = cfg->border_color, .width = {0, 0, 0, 1, 0}},
    });

    // Title text
    if (cfg->title) {
        gui_text(cfg->title, &(gui_text_cfg){
            .color = {180, 180, 185, 255},
            .size = font_size,
            .font = cfg->font,
        });
    }

    // Spacer between title and close button
    gui_spacer();

    // Close button
    gui_button_state close_btn = gui_button_begin(&(gui_button_cfg){
        .color       = {60, 60, 65, 0},
        .hover_color = {180, 60, 60, 255},
        .press_color = {200, 40, 40, 255},
        .corner_radius = 3,
        .width = 20,
        .height = 20,
    });
    Clay_Color close_fg = (close_btn.hovered || close_btn.pressed)
                        ? (Clay_Color){255, 255, 255, 255}
                        : (Clay_Color){140, 140, 145, 255};
    gui_text("x", &(gui_text_cfg){.color = close_fg, .size = font_size, .font = cfg->font});
    gui_button_end();

    result.close_clicked = close_btn.clicked;

    Clay__CloseElement(); // header

    // Start drag: header hovered (previous frame) && mouse just pressed && close button not hovered
    if (!state->dragging && input_mouse_pressed(MOUSE_LEFT) && header_hovered && !close_btn.hovered) {
        state->dragging = true;
    }

    // Body is left open — caller places children, then calls gui_window_end()
    return result;
}

void gui_window_end(void) {
    // Close the outer panel
    Clay__CloseElement();
}
