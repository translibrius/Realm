#pragma once

#include "defines.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "gl_render_target.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"
#include "platform/platform.h"
#include "gl_mesh.h"
#include "asset/font.h"
#include "core/camera.h"
#include "renderer/frame_data.h"

typedef struct {
    u32 texture_id;
    rl_font *font;
    const rl_glyph *glyph_map[256]; // direct lookup by codepoint for ASCII range
} GL_Font;

DA_DEFINE(GL_Fonts, GL_Font);

typedef struct GL_TextPipeline {
    u32 vao;
    u32 vbo;
    GL_Shader shader;
    i32 loc_font_atlas;
    i32 loc_screen_size;
    i32 loc_px_range;
} GL_TextPipeline;

typedef struct GL_TextVertex {
    vec2 pos;
    vec2 uv;
    vec4 color;
} GL_TextVertex;

typedef struct GL_GuiVertex {
    vec2 pos;
    vec2 uv;
    vec4 color;
    vec4 rect_info;     // (half_w, half_h, has_rounding, 0)
    vec4 corner_radii;  // (topLeft, topRight, bottomLeft, bottomRight)
} GL_GuiVertex;

typedef struct GL_GuiPipeline {
    u32 vao;
    u32 vbo;
    GL_Shader shader;
    i32 loc_screen_size;
    i32 loc_font_atlas;
    i32 loc_px_range;
    i32 loc_weight;
} GL_GuiPipeline;

typedef struct GL_Context {
    platform_window *window;
    rl_arena arena;

    // Text
    GL_TextPipeline text_pipeline;
    GL_Fonts fonts;
    rl_font *active_font;

    // GUI
    GL_GuiPipeline gui_pipeline;

    // Defaults
    GL_Shader default_shader;
    GL_Shader light_shader;
    GL_Mesh cube_mesh;

    // Grid
    GL_Shader grid_shader;
    u32 grid_vao;

    // Texture lookup (asset_id -> GL_Texture)
    enum { GL_MAX_TEXTURES = 64 };
    struct { asset_id asset_id; GL_Texture texture; } textures[64];
    u32 texture_count;

    // Model GPU cache (asset_id -> array of GL_Mesh, one per sub-mesh)
    struct { asset_id model_id; GL_Mesh *meshes; u32 mesh_count; } model_cache[64];
    u32 model_cache_count;

    // Clear color (default from RL_CLEAR_COLOR_*)
    f32 clear_color[4];

    // Line rendering (debug lines, frustum viz)
    u32 line_vao;
    u32 line_vbo;

    // Debug
    b8 debug_wireframe;

    // Outline (JFA)
    GL_Shader outline_mask_shader;
    GL_Shader jfa_init_shader;
    GL_Shader jfa_step_shader;
    GL_Shader outline_composite_shader;
    GL_RenderTarget outline_mask_rt;  // RGBA8 — white silhouette
    GL_RenderTarget outline_jfa_a;    // RGBA16F — JFA ping
    GL_RenderTarget outline_jfa_b;    // RGBA16F — JFA pong
    u32 fullscreen_quad_vao;
    u32 fullscreen_quad_vbo;
    b8 outline_ready; // true once shaders + RTs are initialized

    // Per-frame outline data (copied from frame_data)
    rl_frame_outline *frame_outlines;
    u32 frame_outline_count;

    // Mat
    mat4 view;
    mat4 projection;
    vec3 pos;
} GL_Context;