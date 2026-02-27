#pragma once

#include "cglm.h"
#include "defines.h"
#include "memory/arena.h"

typedef struct rl_font rl_font;

// Draw command types
typedef enum rl_ui_cmd_type {
    RL_UI_CMD_QUAD,
    RL_UI_CMD_TEXT,
} rl_ui_cmd_type;

typedef struct rl_ui_cmd_quad {
    f32 x, y, w, h;
    vec4 color;
} rl_ui_cmd_quad;

typedef struct rl_ui_cmd_text {
    const char *text;
    rl_font *font;
    f32 x, y;
    f32 size_px;
    vec4 color;
} rl_ui_cmd_text;

typedef struct rl_ui_cmd {
    rl_ui_cmd_type type;
    union {
        rl_ui_cmd_quad quad;
        rl_ui_cmd_text text;
    };
} rl_ui_cmd;

typedef struct rl_ui_draw_list {
    rl_ui_cmd *commands;
    u32 count;
    rl_arena *arena;
    f32 screen_w;
    f32 screen_h;
} rl_ui_draw_list;

// Frame lifecycle
REALM_API void rl_ui_begin(rl_arena *arena, f32 screen_w, f32 screen_h);
REALM_API rl_ui_draw_list *rl_ui_end(void);

// Widgets
REALM_API void rl_ui_label(f32 x, f32 y, const char *text, f32 size_px, vec4 color);
REALM_API b8 rl_ui_button(f32 x, f32 y, f32 w, f32 h, const char *label);
REALM_API b8 rl_ui_slider_f32(f32 x, f32 y, f32 w, const char *label, f32 *value, f32 min, f32 max);
REALM_API void rl_ui_panel(f32 x, f32 y, f32 w, f32 h, vec4 color);
REALM_API b8 rl_ui_checkbox(f32 x, f32 y, const char *label, b8 *value);
REALM_API void rl_ui_separator(f32 x, f32 y, f32 w);
REALM_API void rl_ui_section(f32 x, f32 *y, f32 w, const char *title);
