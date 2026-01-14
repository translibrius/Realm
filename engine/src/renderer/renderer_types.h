#pragma once
#include "defines.h"

#include "platform/platform.h"
#include "asset/font.h"
#include "core/camera.h"
#include "memory/containers/dynamic_array.h"

#include "cglm.h"

#define MAX_TEXT_GLYPHS 256

typedef struct vertex {
    vec2 pos;
    vec3 color;
} vertex;

DA_DEFINE(Vertices, vertex);
DA_DEFINE(Indices, u16);

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
    BACKEND_VULKAN
} RENDERER_BACKEND;

typedef enum SHADER_TYPE {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
    SHADER_TYPE_COMPUTE,
    SHADER_TYPE_GEOMETRY,
    SHADER_TYPE_TESS_CONTROL,
    SHADER_TYPE_TESS_EVAL,

    SHADER_TYPE_UNKNOWN
} SHADER_TYPE;

typedef struct renderer_interface {
    b8 (*initialize)(platform_window *window, rl_camera *camera, b8 vsync);
    void (*shutdown)();
    void (*begin_frame)(f64 delta_time);
    void (*end_frame)();
    void (*swap_buffers)();
    void (*render_text)(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
    void (*set_active_font)(rl_font *font);
    void (*set_view_projection)(mat4 view, mat4 projection);
} renderer_interface;