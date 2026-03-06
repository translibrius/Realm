#pragma once
#include "defines.h"

#include "renderer/renderer_backend.h"
#include "renderer/frame_data.h"

#include "asset/font.h"
#include "platform/platform.h"

#include "cglm.h"

#define MAX_TEXT_GLYPHS 256

typedef struct vertex {
    vec3 pos;
    vec3 normal;
    vec2 tex_coord;
} vertex;

// Uniform Buffer Object (std140 layout — vec4 alignment)
typedef struct ubo {
    mat4 view;
    mat4 proj;
    vec4 light_pos;      // xyz = position, w = unused
    vec4 light_ambient;  // xyz = ambient,  w = unused
    vec4 light_diffuse;  // xyz = diffuse,  w = unused
    vec4 light_specular; // xyz = specular, w = unused
    vec4 camera_pos;     // xyz = position, w = unused
} ubo;

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
    b8 (*initialize)(platform_window *window, b8 vsync);
    void (*shutdown)();
    void (*begin_frame)(f64 delta_time);
    void (*end_frame)();
    void (*swap_buffers)();
    void (*set_vsync)(b8 vsync);
    void (*render_text)(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
    void (*set_active_font)(rl_font *font);
    void (*set_view_projection)(mat4 view, mat4 projection, vec3 pos);

    platform_window *(*get_active_window)();
    void (*set_active_window)(platform_window *window);
    void (*resize_framebuffer)(i32 w, i32 h);
    void (*submit_frame_data)(rl_frame_data *frame_data);
    void (*submit_gui_data)(void *commands, i32 command_count);
    void (*set_wireframe)(b8 enabled);
} renderer_interface;
