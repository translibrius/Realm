#pragma once

#include "defines.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"
#include "platform/platform.h"
#include "gl_mesh.h"
#include "asset/font.h"
#include "core/camera.h"

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

typedef struct GL_GuiPipeline {
    u32 vao;
    u32 vbo;
    GL_Shader shader;
    i32 loc_screen_size;
    i32 loc_font_atlas;
    i32 loc_px_range;
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
    GL_Texture wood_texture;
    GL_Texture wood_texture2;
    GL_Mesh cube_mesh;

    // Mat
    mat4 view;
    mat4 projection;
    vec3 pos;
} GL_Context;