#pragma once
#include "defines.h"

#include "platform/platform.h"
#include "asset/font.h"

#include "../vendor/cglm/cglm.h"

#define MAX_TEXT_GLYPHS 256

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
} RENDERER_BACKEND;

typedef struct renderer_interface {
    b8 (*initialize)(platform_window *window);
    void (*shutdown)();
    void (*begin_frame)(f64 delta_time);
    void (*end_frame)();
    void (*swap_buffers)();
    void (*render_text)(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
    void (*set_active_font)(rl_font *font);
} renderer_interface;
