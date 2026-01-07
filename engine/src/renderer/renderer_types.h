#pragma once
#include "defines.h"

#include "platform/platform.h"
#include "asset/font.h"
#include "core/camera.h"

#include "../vendor/cglm/cglm.h"

#define MAX_TEXT_GLYPHS 256

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
    BACKEND_VULKAN
} RENDERER_BACKEND;

typedef struct renderer_interface {
    b8 (*initialize)(platform_window *window, rl_camera *camera);
    void (*shutdown)();
    void (*begin_frame)(f64 delta_time);
    void (*end_frame)();
    void (*swap_buffers)();
    void (*render_text)(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
    void (*set_active_font)(rl_font *font);
    void (*set_view_projection)(mat4 view, mat4 projection);
} renderer_interface;