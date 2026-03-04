#include "gui/gui_window.h"

#include "gui/gui_button.h"
#include "gui/gui_text.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

#include <string.h>

static Clay_ElementId gui__make_id(const char *id) {
    Clay_String s = {.length = (i32)strlen(id), .chars = id};
    return Clay__HashString(s, 0);
}

// We need unique sub-IDs for header, close button, etc.
// Build them by appending suffixes to the base ID.
static Clay_ElementId gui__make_sub_id(const char *id, const char *suffix) {
    char buf[128];
    i32 id_len = (i32)strlen(id);
    i32 suffix_len = (i32)strlen(suffix);
    i32 total = id_len + suffix_len;
    if (total >= 128) total = 127;
    memcpy(buf, id, id_len);
    memcpy(buf + id_len, suffix, suffix_len);
    buf[total] = '\0';
    Clay_String s = {.length = total, .chars = buf};
    return Clay__HashString(s, 0);
}

b8 gui_window_begin(const char *id, gui_window_state *state, const gui_window_cfg *cfg) {
    if (!state || !state->visible || !cfg) {
        return false;
    }

    f32 corner_radius = cfg->corner_radius > 0 ? cfg->corner_radius : 6;
    u16 font_size = cfg->font_size > 0 ? cfg->font_size : 13;
    i32 z_index = cfg->z_index > 0 ? cfg->z_index : 100;

    // Window bounds clamping
    platform_window *win = renderer_get_active_window();
    f32 win_w = win ? (f32)win->settings.width : 700.0f;
    f32 win_h = win ? (f32)win->settings.height : 400.0f;

    // Close button state (query from previous frame's element)
    Clay_ElementId close_eid = gui__make_sub_id(id, "Close");
    gui_button_state close_btn = {0};
    close_btn.hovered = Clay_PointerOver(close_eid);

    static u32 close_pressed_id;
    if (input_mouse_pressed(MOUSE_LEFT) && close_btn.hovered) {
        close_pressed_id = close_eid.id;
    }
    close_btn.pressed = close_btn.hovered && close_pressed_id == close_eid.id;
    if (close_pressed_id == close_eid.id && !input_is_mouse_down(MOUSE_LEFT)) {
        close_pressed_id = 0;
        if (close_btn.hovered) {
            close_btn.clicked = true;
        }
    }

    if (close_btn.clicked) {
        state->visible = false;
        return false;
    }

    // Header drag
    Clay_ElementId header_eid = gui__make_sub_id(id, "Header");
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
    } else if (mouse_down && !close_btn.pressed && Clay_PointerOver(header_eid)) {
        state->dragging = true;
    }

    // Clamp to window bounds
    f32 max_x = (win_w - cfg->width) * 0.5f;
    if (max_x < 0) max_x = 0;
    if (state->pos_x < -max_x) state->pos_x = -max_x;
    if (state->pos_x > max_x) state->pos_x = max_x;
    if (state->pos_y < 0) state->pos_y = 0;
    if (state->pos_y + cfg->height > win_h) state->pos_y = win_h - cfg->height;

    // === Outer panel (floating) ===
    Clay_ElementId panel_eid = gui__make_id(id);
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
    Clay__OpenElementWithId(header_eid);
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
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {.width = CLAY_SIZING_GROW(0)}},
    });
    Clay__CloseElement();

    // Close button
    Clay_Color close_bg = close_btn.pressed  ? (Clay_Color){200, 40, 40, 255}
                        : close_btn.hovered  ? (Clay_Color){180, 60, 60, 255}
                                             : (Clay_Color){60, 60, 65, 0};
    Clay_Color close_fg = (close_btn.hovered || close_btn.pressed)
                        ? (Clay_Color){255, 255, 255, 255}
                        : (Clay_Color){140, 140, 145, 255};

    Clay__OpenElementWithId(close_eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(20), .height = CLAY_SIZING_FIXED(20)},
            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = close_bg,
        .cornerRadius = CLAY_CORNER_RADIUS(3),
    });
    gui_text("x", &(gui_text_cfg){.color = close_fg, .size = font_size, .font = cfg->font});
    Clay__CloseElement(); // close button

    Clay__CloseElement(); // header

    // Body is left open — caller places children, then calls gui_window_end()
    return true;
}

void gui_window_end(void) {
    // Close the outer panel
    Clay__CloseElement();
}
