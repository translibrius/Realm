#include "ui/ui.h"

#include "platform/input.h"

#include <stdio.h>
#include <string.h>

typedef struct rl_ui_ctx {
    rl_ui_draw_list draw_list;

    // Input state (cached at begin)
    vec2 mouse_pos;
    b8 mouse_down;
    b8 mouse_pressed;

    // Widget interaction
    u32 hot_id;
    u32 active_id;
    u32 next_id;
} rl_ui_ctx;

static rl_ui_ctx ctx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static b8 point_in_rect(vec2 p, f32 x, f32 y, f32 w, f32 h) {
    return p[0] >= x && p[0] <= x + w && p[1] >= y && p[1] <= y + h;
}

static rl_ui_cmd *ui_push_cmd(void) {
    rl_ui_cmd *cmd = rl_arena_push_aligned(
        ctx.draw_list.arena, sizeof(rl_ui_cmd), _Alignof(rl_ui_cmd), false);
    if (!cmd) {
        return nullptr;
    }
    if (ctx.draw_list.count == 0) {
        ctx.draw_list.commands = cmd;
    }
    ctx.draw_list.count++;
    return cmd;
}

static void ui_push_quad(f32 x, f32 y, f32 w, f32 h, vec4 color) {
    rl_ui_cmd *cmd = ui_push_cmd();
    if (!cmd) return;
    cmd->type = RL_UI_CMD_QUAD;
    cmd->quad = (rl_ui_cmd_quad){.x = x, .y = y, .w = w, .h = h};
    glm_vec4_copy(color, cmd->quad.color);
}

static void ui_push_text(const char *text, rl_font *font, f32 x, f32 y, f32 size_px, vec4 color) {
    rl_ui_cmd *cmd = ui_push_cmd();
    if (!cmd) return;
    cmd->type = RL_UI_CMD_TEXT;
    cmd->text = (rl_ui_cmd_text){
        .text = text,
        .font = font,
        .x = x,
        .y = y,
        .size_px = size_px,
    };
    glm_vec4_copy(color, cmd->text.color);
}

static char *ui_arena_printf(const char *fmt, ...) {
    char tmp[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len <= 0) return nullptr;

    u32 size = (u32)len + 1;
    char *buf = rl_arena_push(ctx.draw_list.arena, size, false);
    if (!buf) return nullptr;
    memcpy(buf, tmp, size);
    return buf;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void rl_ui_begin(rl_arena *arena, f32 screen_w, f32 screen_h) {
    // Cache input
    input_get_mouse_position(ctx.mouse_pos);
    ctx.mouse_down = input_is_mouse_down(MOUSE_LEFT);
    ctx.mouse_pressed = input_mouse_pressed(MOUSE_LEFT);

    // Reset draw list
    ctx.draw_list.arena = arena;
    ctx.draw_list.screen_w = screen_w;
    ctx.draw_list.screen_h = screen_h;
    ctx.draw_list.count = 0;
    ctx.draw_list.commands = nullptr;

    // Reset per-frame state
    ctx.hot_id = 0;
    ctx.next_id = 1;
}

rl_ui_draw_list *rl_ui_end(void) {
    return &ctx.draw_list;
}

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------

void rl_ui_panel(f32 x, f32 y, f32 w, f32 h, vec4 color) {
    ui_push_quad(x, y, w, h, color);
}

void rl_ui_label(f32 x, f32 y, const char *text, f32 size_px, vec4 color) {
    ui_push_text(text, nullptr, x, y, size_px, color);
}

void rl_ui_separator(f32 x, f32 y, f32 w) {
    ui_push_quad(x, y, w, 1.0f, (vec4){0.3f, 0.3f, 0.35f, 0.6f});
}

void rl_ui_section(f32 x, f32 *y, f32 w, const char *title) {
    rl_ui_separator(x, *y, w);
    *y += 4.0f;
    ui_push_text(title, nullptr, x, *y, 11.0f, (vec4){0.55f, 0.7f, 1.0f, 1.0f});
    *y += 14.0f;
}

b8 rl_ui_button(f32 x, f32 y, f32 w, f32 h, const char *label) {
    u32 id = ctx.next_id++;
    b8 hovered = point_in_rect(ctx.mouse_pos, x, y, w, h);
    b8 clicked = false;

    if (hovered) {
        ctx.hot_id = id;
        if (ctx.mouse_pressed) {
            ctx.active_id = id;
        }
    }

    if (ctx.active_id == id && !ctx.mouse_down) {
        if (hovered) {
            clicked = true;
        }
        ctx.active_id = 0;
    }

    // Background color
    vec4 bg;
    if (ctx.active_id == id) {
        glm_vec4_copy((vec4){0.15f, 0.15f, 0.18f, 0.95f}, bg);
    } else if (hovered) {
        glm_vec4_copy((vec4){0.35f, 0.35f, 0.40f, 0.9f}, bg);
    } else {
        glm_vec4_copy((vec4){0.25f, 0.25f, 0.28f, 0.9f}, bg);
    }

    ui_push_quad(x, y, w, h, bg);
    ui_push_text(label, nullptr, x + 8.0f, y + 4.0f, 14.0f, (vec4){1.0f, 1.0f, 1.0f, 1.0f});

    return clicked;
}

b8 rl_ui_slider_f32(f32 x, f32 y, f32 w, const char *label, f32 *value, f32 min, f32 max) {
    u32 id = ctx.next_id++;
    b8 changed = false;

    // Layout: [label 60px] [track ...] [value 45px]
    const f32 h = 14.0f;
    const f32 label_w = 60.0f;
    const f32 value_w = 45.0f;
    const f32 gap = 4.0f;
    const f32 track_x = x + label_w + gap;
    const f32 track_w = w - label_w - value_w - gap * 2.0f;
    const f32 track_h = 6.0f;
    const f32 track_y = y + (h - track_h) * 0.5f;
    const f32 handle_w = 4.0f;

    // Hit area covers the track region
    b8 hovered = point_in_rect(ctx.mouse_pos, track_x, y, track_w, h);

    if (hovered && ctx.mouse_down && ctx.active_id == 0) {
        ctx.active_id = id;
    }

    if (ctx.active_id == id) {
        if (ctx.mouse_down) {
            f32 t = (ctx.mouse_pos[0] - track_x) / track_w;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            *value = min + t * (max - min);
            changed = true;
        } else {
            ctx.active_id = 0;
        }
    }

    f32 range = max - min;
    f32 t = (range > 0.0f) ? (*value - min) / range : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // Label (left)
    ui_push_text(label, nullptr, x, y + 1.0f, 10.0f, (vec4){0.6f, 0.6f, 0.65f, 1.0f});

    // Track background
    vec4 track_bg;
    if (hovered || ctx.active_id == id) {
        glm_vec4_copy((vec4){0.25f, 0.25f, 0.30f, 1.0f}, track_bg);
    } else {
        glm_vec4_copy((vec4){0.20f, 0.20f, 0.24f, 1.0f}, track_bg);
    }
    ui_push_quad(track_x, track_y, track_w, track_h, track_bg);

    // Filled portion
    f32 fill_w = t * track_w;
    if (fill_w > 0.5f) {
        vec4 fill_color;
        if (ctx.active_id == id) {
            glm_vec4_copy((vec4){0.45f, 0.65f, 1.0f, 1.0f}, fill_color);
        } else {
            glm_vec4_copy((vec4){0.35f, 0.55f, 0.9f, 0.9f}, fill_color);
        }
        ui_push_quad(track_x, track_y, fill_w, track_h, fill_color);
    }

    // Handle
    f32 handle_x = track_x + t * (track_w - handle_w);
    vec4 handle_color;
    if (ctx.active_id == id) {
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, handle_color);
    } else if (hovered) {
        glm_vec4_copy((vec4){0.9f, 0.9f, 0.95f, 1.0f}, handle_color);
    } else {
        glm_vec4_copy((vec4){0.75f, 0.75f, 0.8f, 1.0f}, handle_color);
    }
    ui_push_quad(handle_x, track_y - 1.0f, handle_w, track_h + 2.0f, handle_color);

    // Value (right)
    char *val_text = ui_arena_printf("%.2f", *value);
    if (val_text) {
        ui_push_text(val_text, nullptr, x + w - value_w, y + 1.0f, 10.0f, (vec4){0.75f, 0.75f, 0.8f, 1.0f});
    }

    return changed;
}

b8 rl_ui_checkbox(f32 x, f32 y, const char *label, b8 *value) {
    u32 id = ctx.next_id++;
    const f32 box_size = 18.0f;
    b8 hovered = point_in_rect(ctx.mouse_pos, x, y, box_size, box_size);
    b8 toggled = false;

    if (hovered) {
        ctx.hot_id = id;
        if (ctx.mouse_pressed) {
            ctx.active_id = id;
        }
    }

    if (ctx.active_id == id && !ctx.mouse_down) {
        if (hovered) {
            *value = !(*value);
            toggled = true;
        }
        ctx.active_id = 0;
    }

    // Box background
    vec4 bg;
    if (*value) {
        glm_vec4_copy((vec4){0.3f, 0.6f, 0.9f, 0.95f}, bg);
    } else if (hovered) {
        glm_vec4_copy((vec4){0.35f, 0.35f, 0.40f, 0.9f}, bg);
    } else {
        glm_vec4_copy((vec4){0.25f, 0.25f, 0.28f, 0.9f}, bg);
    }
    ui_push_quad(x, y, box_size, box_size, bg);

    // Check mark (text "x" when checked)
    if (*value) {
        ui_push_text("x", nullptr, x + 3.0f, y + 1.0f, 14.0f, (vec4){1.0f, 1.0f, 1.0f, 1.0f});
    }

    // Label
    ui_push_text(label, nullptr, x + box_size + 6.0f, y + 1.0f, 14.0f, (vec4){0.8f, 0.8f, 0.8f, 1.0f});

    return toggled;
}
